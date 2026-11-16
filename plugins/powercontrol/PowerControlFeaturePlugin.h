/*
 * PowerControlFeaturePlugin.h - declaration of PowerControlFeaturePlugin class
 *
 * Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
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
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include "CommandLineIO.h"
#include "CommandLinePluginInterface.h"
#include "Feature.h"
#include "FeatureProviderInterface.h"

class PowerControlFeaturePlugin : public QObject,
		PluginInterface,
		CommandLineIO,
		CommandLinePluginInterface,
		FeatureProviderInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.PowerControl")
	Q_INTERFACES(PluginInterface CommandLinePluginInterface FeatureProviderInterface)
public:
	enum class Argument
	{
		ShutdownTimeout,
		// ITLabControl: MAC addresses a powered-on computer broadcasts WOL packets for in its own network
		MacAddresses,
		// ITLabControl: network data of a student computer for Wake-on-LAN (sent once per connection)
		IpAddress,
		PrefixLength,
		// ITLabControl: target addresses (directed broadcasts / last known IPs) for relayed WOL packets
		TargetAddresses
	};
	Q_ENUM(Argument)

	// ITLabControl: commands of the "Power on" feature
	enum class PowerOnCommand
	{
		RelayWakeOnLan = 0, // = FeatureMessage::Command::Default, as sent by earlier versions
		NetworkInfo = 1
	};
	Q_ENUM(PowerOnCommand)

	explicit PowerControlFeaturePlugin( QObject* parent = nullptr );
	~PowerControlFeaturePlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("4122e8ca-b617-4e36-b851-8e050ed2d82e") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 2 );
	}

	QString name() const override
	{
		return QStringLiteral("PowerControl");
	}

	QString description() const override
	{
		return tr( "Power on/down or reboot a computer" );
	}

	QString vendor() const override
	{
		return QStringLiteral("ITLabControl contributors");
	}

	QString copyright() const override
	{
		return QStringLiteral("Tobias Junghans");
	}

	QString commandLineModuleName() const override
	{
		return QStringLiteral( "power" );
	}

	QString commandLineModuleHelp() const override
	{
		return tr( "Commands for controlling power status of computers" );
	}

	QStringList commands() const override;
	QString commandHelp( const QString& command ) const override;

	const FeatureList& featureList() const override;

	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool handleFeatureMessage( VeyonServerInterface& server,
							   const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

	// ITLabControl: learn MAC/IP of student computers for Wake-on-LAN
	bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
							   const FeatureMessage& message ) override;
	void sendAsyncFeatureMessages( VeyonServerInterface& server, const MessageContext& messageContext ) override;

public Q_SLOTS:
	CommandLinePluginInterface::RunResult handle_help( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_on( const QStringList& arguments );

protected:
	bool eventFilter(QObject* watched, QEvent* event) override;

private:
	bool confirmFeatureExecution( const Feature& feature, bool all, QWidget* parent );
	static bool broadcastWOLPacket( QString macAddress, const QStringList& extraTargetAddresses = {} );

	// ITLabControl: MAC address and extra target addresses (directed broadcast of the computer's
	// network, last known IP) for a computer, using what was learned while it was connected
	struct WakeOnLanTarget
	{
		QString macAddress;
		QStringList targetAddresses;
	};
	static WakeOnLanTarget wakeOnLanTarget( const Computer& computer );
	static void storeNetworkInfo( const Computer& computer, const QString& macAddress,
								  const QString& ipAddress, int prefixLength );
	void relayWakeOnLan( VeyonMasterInterface& master, const ComputerControlInterfaceList& computerControlInterfaces );

	void confirmShutdown();
	void displayShutdownTimeout( int shutdownTimeout );

	QMap<QString, QString> m_commands;

	const Feature m_powerOnFeature;
	const Feature m_rebootFeature;
	const Feature m_powerDownFeature;
	const Feature m_powerDownNowFeature;
	const Feature m_installUpdatesAndPowerDownFeature;
	const Feature m_powerDownConfirmedFeature;
	const Feature m_powerDownDelayedFeature;
	const FeatureList m_features;

};
