/*
 * ItlabRestrictionsPlugin.h - restrictions for student computers (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 *  "Restrictions" button of the Teacher Console (phase 2, docs/ITLabControl_Design_Plan.md):
 *    - block all websites / allow only listed websites / block listed websites / unblock websites
 *    - remove all restrictions
 *
 *  MASTER --(apply / heartbeat)--> SERVER (SYSTEM rights) applies browser policies and firewall rules
 *  SERVER --(status, once per connection)--> MASTER, so a restarted Console keeps sending heartbeats
 *
 *  Restrictions are removed automatically on the student computer when the Teacher Console has not
 *  sent a heartbeat for ITLab/RestrictionHeartbeatTimeout minutes (default 15) or after
 *  ITLab/RestrictionMaxDuration minutes (default 240). Emergency: "veyon-cli restrictions clear".
 */

#pragma once

#include <QMutex>
#include <QTimer>

#include "CommandLineIO.h"
#include "CommandLinePluginInterface.h"
#include "Feature.h"
#include "FeatureProviderInterface.h"
#include "RestrictionsBackend.h"

class ItlabRestrictionsPlugin : public QObject,
		FeatureProviderInterface,
		PluginInterface,
		CommandLineIO,
		CommandLinePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "vn.itlabcontrol.Plugins.ItlabRestrictions")
	Q_INTERFACES(PluginInterface FeatureProviderInterface CommandLinePluginInterface)

public:
	enum class FeatureCommand {
		ApplyWebFilter,
		RemoveWebFilter,
		RemoveAllRestrictions,
		Heartbeat,
		Status,
		ApplyAppControl,
		RemoveAppControl,
		RefreshUserPolicy,
	};
	Q_ENUM(FeatureCommand)

	enum class Argument {
		WebMode,
		Websites,
		Active,
		AppMode,
		Apps,
	};
	Q_ENUM(Argument)

	explicit ItlabRestrictionsPlugin( QObject* parent = nullptr );
	~ItlabRestrictionsPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("5c1f2a7e-8b3d-4e6a-9f10-2d7c4b8e6a31") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("ItlabRestrictions");
	}

	QString description() const override
	{
		return tr( "Restrict student computers, e.g. block websites" );
	}

	QString vendor() const override
	{
		return QStringLiteral("ITLabControl");
	}

	QString copyright() const override
	{
		return QStringLiteral("ITLabControl contributors");
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	// MASTER
	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;
	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						 const ComputerControlInterfaceList& computerControlInterfaces ) override;
	bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
							   const FeatureMessage& message ) override;

	// SERVER
	bool handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
							   const FeatureMessage& message ) override;
	void sendAsyncFeatureMessages( VeyonServerInterface& server, const MessageContext& messageContext ) override;
	bool isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const override;

	// WORKER (session of the student): refresh the user policies so browsers reload them sooner
	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

	// COMMAND LINE: veyon-cli restrictions status|clear
	QString commandLineModuleName() const override
	{
		return QStringLiteral("restrictions");
	}
	QString commandLineModuleHelp() const override
	{
		return tr( "Show or remove the restrictions of this computer" );
	}
	QStringList commands() const override;
	QString commandHelp( const QString& command ) const override;

public Q_SLOTS:
	CommandLinePluginInterface::RunResult handle_status( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_clear( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_web( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_apps( const QStringList& arguments );

private:
	static constexpr int HeartbeatInterval = 60 * 1000;
	static constexpr int ExpiryCheckInterval = 30 * 1000;
	static constexpr int AppWatchInterval = 700;

	// MASTER
	void addHeartbeatTargets( const ComputerControlInterfaceList& computerControlInterfaces );
	void sendHeartbeats();

	// SERVER
	void initServer();
	void checkExpiry();
	void updateActiveState( const QJsonObject& state );
	void enforceAppControl();

	const Feature m_restrictionsFeature;
	const Feature m_webRestrictionsFeature;
	const Feature m_blockAllWebsitesFeature;
	const Feature m_allowListedWebsitesFeature;
	const Feature m_blockListedWebsitesFeature;
	const Feature m_unblockWebsitesFeature;
	const Feature m_appRestrictionsFeature;
	const Feature m_blockListedAppsFeature;
	const Feature m_allowListedAppsFeature;
	const Feature m_unblockAppsFeature;
	const Feature m_removeAllRestrictionsFeature;
	const FeatureList m_features;

	QMap<QString, QString> m_commands;

	// MASTER
	QTimer m_heartbeatTimer;
	QList<QWeakPointer<ComputerControlInterface>> m_heartbeatTargets;

	// SERVER
	QTimer m_expiryCheckTimer;
	mutable QMutex m_stateMutex;
	bool m_active{false};
	RestrictionsBackend::WebMode m_webMode{RestrictionsBackend::WebMode::None};
	RestrictionsBackend::AppMode m_appMode{RestrictionsBackend::AppMode::None};
	QStringList m_apps;
	qint64 m_lastHeartbeatWrite{0};
	QTimer m_appWatchTimer;
	VeyonServerInterface* m_server{nullptr};
	QMap<QString, qint64> m_lastAppNotification;

};
