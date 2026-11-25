/*
 * ItlabAudioPlugin.h - mute, unmute and set the volume of student computers (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 *  "Sound" button of the Teacher Console:
 *    - Mute (keep muted): the student computer stays muted, also if the student unmutes it
 *    - Unmute
 *    - Volume 25 / 50 / 75 / 100 % and "Set volume..."
 *
 *  MASTER --> SERVER --> WORKER (session of the student, Windows Core Audio API)
 *  The worker of the mute feature keeps running while the computer is muted (shown as active
 *  feature in the Teacher Console); other workers exit after applying the change.
 */

#pragma once

#include <QTimer>

#include "CommandLineIO.h"
#include "CommandLinePluginInterface.h"
#include "Feature.h"
#include "FeatureProviderInterface.h"

class ItlabAudioPlugin : public QObject, FeatureProviderInterface, PluginInterface,
		CommandLineIO, CommandLinePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "vn.itlabcontrol.Plugins.ItlabAudio")
	Q_INTERFACES(PluginInterface FeatureProviderInterface CommandLinePluginInterface)

public:
	enum class FeatureCommand {
		Mute,
		Unmute,
		SetVolume,
	};
	Q_ENUM(FeatureCommand)

	enum class Argument {
		Volume,
	};
	Q_ENUM(Argument)

	explicit ItlabAudioPlugin( QObject* parent = nullptr );
	~ItlabAudioPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("8e3c5a71-2f4b-4d96-b1a0-7c6d5e4f3a29") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("ItlabAudio");
	}

	QString description() const override
	{
		return tr( "Mute, unmute and set the volume of student computers" );
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

	// SERVER
	bool handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	// WORKER
	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

	// COMMAND LINE (this computer): veyon-cli audio status|mute|unmute|volume <0-100>
	QString commandLineModuleName() const override
	{
		return QStringLiteral("audio");
	}
	QString commandLineModuleHelp() const override
	{
		return tr( "Mute, unmute or set the volume of this computer" );
	}
	QStringList commands() const override
	{
		return { QStringLiteral("status"), QStringLiteral("mute"), QStringLiteral("unmute"), QStringLiteral("volume") };
	}
	QString commandHelp( const QString& command ) const override
	{
		return command == QLatin1String("volume") ? tr( "Set the volume (0 - 100)" ) : QString{};
	}

public Q_SLOTS:
	CommandLinePluginInterface::RunResult handle_status( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_mute( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_unmute( const QStringList& arguments );
	CommandLinePluginInterface::RunResult handle_volume( const QStringList& arguments );

private:
	static bool readState( bool* muted, int* percent );
	static bool setMuted( bool muted );
	static bool setVolume( int percent );
	static void quitWorkerSoon();

	const Feature m_audioFeature;
	const Feature m_muteFeature;
	const Feature m_unmuteFeature;
	const Feature m_volume25Feature;
	const Feature m_volume50Feature;
	const Feature m_volume75Feature;
	const Feature m_volume100Feature;
	const Feature m_setVolumeFeature;
	const FeatureList m_features;

	// WORKER: re-applies the mute while the computer has to stay muted
	QTimer m_keepMutedTimer;

};
