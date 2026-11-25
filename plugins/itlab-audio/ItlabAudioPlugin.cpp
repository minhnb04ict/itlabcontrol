/*
 * ItlabAudioPlugin.cpp - mute, unmute and set the volume of student computers (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

#include <QCoreApplication>
#include <QInputDialog>
#include <QMessageBox>

#include "ComputerControlInterface.h"
#include "FeatureWorkerManager.h"
#include "ItlabAudioPlugin.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"


namespace {

// runs the function with the volume control of the default output device (speakers/headphones)
template<typename Function>
bool withEndpointVolume( Function function )
{
	const auto comResult = CoInitializeEx( nullptr, COINIT_APARTMENTTHREADED );

	IMMDeviceEnumerator* enumerator = nullptr;
	IMMDevice* device = nullptr;
	IAudioEndpointVolume* endpointVolume = nullptr;
	bool success = false;

	if( SUCCEEDED( CoCreateInstance( __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
									 __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>( &enumerator ) ) ) &&
		SUCCEEDED( enumerator->GetDefaultAudioEndpoint( eRender, eConsole, &device ) ) &&
		SUCCEEDED( device->Activate( __uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
									 reinterpret_cast<void**>( &endpointVolume ) ) ) )
	{
		success = function( endpointVolume );
	}

	if( endpointVolume ) endpointVolume->Release();
	if( device ) device->Release();
	if( enumerator ) enumerator->Release();
	if( SUCCEEDED( comResult ) )
	{
		CoUninitialize();
	}

	return success;
}

}



ItlabAudioPlugin::ItlabAudioPlugin( QObject* parent ) :
	QObject( parent ),
	m_audioFeature( QStringLiteral("ItlabAudio"),
					Feature::Flag::Action | Feature::Flag::AllComponents,
					Feature::Uid( "3f6b9d12-7a4e-4c85-9e21-5b8c7d6a4f30" ),
					Feature::Uid(),
					tr( "Sound" ), {},
					tr( "Mute or unmute the student computers or change their volume." ),
					QStringLiteral(":/itlab-audio/itlab-audio.png") ),
	m_muteFeature( QStringLiteral("ItlabAudioMute"),
				   Feature::Flag::Action | Feature::Flag::AllComponents,
				   Feature::Uid( "a7c1e5f3-4b2d-4e69-8a0f-6d5e4c3b2a18" ),
				   m_audioFeature.uid(),
				   tr( "Mute (keep muted)" ), {},
				   tr( "Mutes the student computers. If a student unmutes, the computer is muted again." ) ),
	m_unmuteFeature( QStringLiteral("ItlabAudioUnmute"),
					 Feature::Flag::Action | Feature::Flag::AllComponents,
					 Feature::Uid( "b8d2f6a4-5c3e-4f7a-9b10-7e6f5d4c3b29" ),
					 m_audioFeature.uid(),
					 tr( "Unmute" ), {},
					 tr( "Turns the sound of the student computers on again." ) ),
	m_volume25Feature( QStringLiteral("ItlabAudioVolume25"),
					   Feature::Flag::Action | Feature::Flag::AllComponents,
					   Feature::Uid( "c9e3a7b5-6d4f-4a8b-8c21-8f7a6e5d4c3a" ),
					   m_audioFeature.uid(), tr( "Volume 25%" ), {}, {} ),
	m_volume50Feature( QStringLiteral("ItlabAudioVolume50"),
					   Feature::Flag::Action | Feature::Flag::AllComponents,
					   Feature::Uid( "d0f4b8c6-7e5a-4b9c-9d32-9a8b7f6e5d4b" ),
					   m_audioFeature.uid(), tr( "Volume 50%" ), {}, {} ),
	m_volume75Feature( QStringLiteral("ItlabAudioVolume75"),
					   Feature::Flag::Action | Feature::Flag::AllComponents,
					   Feature::Uid( "e1a5c9d7-8f6b-4cad-8e43-ab9c8a7f6e5c" ),
					   m_audioFeature.uid(), tr( "Volume 75%" ), {}, {} ),
	m_volume100Feature( QStringLiteral("ItlabAudioVolume100"),
						Feature::Flag::Action | Feature::Flag::AllComponents,
						Feature::Uid( "f2b6dae8-9a7c-4dbe-9f54-bcad9b8a7f6d" ),
						m_audioFeature.uid(), tr( "Volume 100%" ), {}, {} ),
	m_setVolumeFeature( QStringLiteral("ItlabAudioSetVolume"),
						Feature::Flag::Action | Feature::Flag::AllComponents,
						Feature::Uid( "0a3c7e19-ab8d-4ecf-8a65-cdbeac9b8a7e" ),
						m_audioFeature.uid(),
						tr( "Set volume..." ), {},
						tr( "Sets the volume of the student computers to a value between 0 and 100%." ) ),
	m_features( { m_audioFeature, m_muteFeature, m_unmuteFeature, m_volume25Feature, m_volume50Feature,
				  m_volume75Feature, m_volume100Feature, m_setVolumeFeature } )
{
	m_keepMutedTimer.setInterval( 1000 );
	connect( &m_keepMutedTimer, &QTimer::timeout, this, []() { setMuted( true ); } );
}



// ═════════════════════════════════════════════════════════════════════════════
//  MASTER
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabAudioPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
									 const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( hasFeature( feature.uid() ) == false || feature == m_audioFeature )
	{
		return false;
	}

	// never change the sound of the teacher computer itself
	auto targets = computerControlInterfaces;
	targets.removeLocalHostInterfaces();
	if( targets.isEmpty() )
	{
		QMessageBox::information( master.mainWindow(), tr( "Sound" ),
								  tr( "The sound of this computer (the teacher computer) is never changed." ) );
		return true;
	}

	QVariantMap arguments;
	if( feature == m_setVolumeFeature )
	{
		bool ok = false;
		const auto volume = QInputDialog::getInt( master.mainWindow(), tr( "Set volume" ),
												  tr( "Volume of the student computers (0 - 100%):" ), 50, 0, 100, 5, &ok );
		if( ok == false )
		{
			return true;
		}
		arguments[argToString( Argument::Volume )] = volume;
	}

	return controlFeature( feature.uid(), Operation::Start, arguments, targets );
}



bool ItlabAudioPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
									   const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( operation != Operation::Start )
	{
		return false;
	}

	if( featureUid == m_muteFeature.uid() )
	{
		sendFeatureMessage( FeatureMessage{ m_muteFeature.uid(), FeatureCommand::Mute }, computerControlInterfaces );
		return true;
	}

	if( featureUid == m_unmuteFeature.uid() )
	{
		sendFeatureMessage( FeatureMessage{ m_muteFeature.uid(), FeatureCommand::Unmute }, computerControlInterfaces );
		return true;
	}

	int volume = -1;
	if( featureUid == m_volume25Feature.uid() ) volume = 25;
	else if( featureUid == m_volume50Feature.uid() ) volume = 50;
	else if( featureUid == m_volume75Feature.uid() ) volume = 75;
	else if( featureUid == m_volume100Feature.uid() ) volume = 100;
	else if( featureUid == m_setVolumeFeature.uid() ) volume = qBound( 0, arguments.value( argToString( Argument::Volume ), 50 ).toInt(), 100 );

	if( volume < 0 )
	{
		return false;
	}

	// changing the volume also ends "keep muted"
	sendFeatureMessage( FeatureMessage{ m_muteFeature.uid(), FeatureCommand::Unmute }, computerControlInterfaces );
	sendFeatureMessage( FeatureMessage{ m_setVolumeFeature.uid(), FeatureCommand::SetVolume }
							.addArgument( Argument::Volume, volume ), computerControlInterfaces );
	return true;
}



// ═════════════════════════════════════════════════════════════════════════════
//  SERVER: the sound belongs to the session of the student, so the worker does the work
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabAudioPlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
											 const FeatureMessage& message )
{
	Q_UNUSED(messageContext)

	if( message.featureUid() != m_muteFeature.uid() && message.featureUid() != m_setVolumeFeature.uid() )
	{
		return false;
	}

	server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );
	return true;
}



// ═════════════════════════════════════════════════════════════════════════════
//  WORKER
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabAudioPlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	Q_UNUSED(worker)

	if( message.featureUid() == m_muteFeature.uid() )
	{
		if( message.command<FeatureCommand>() == FeatureCommand::Mute )
		{
			setMuted( true );
			m_keepMutedTimer.start();
		}
		else
		{
			m_keepMutedTimer.stop();
			setMuted( false );
			quitWorkerSoon();
		}
		return true;
	}

	if( message.featureUid() == m_setVolumeFeature.uid() )
	{
		setVolume( message.argument( Argument::Volume ).toInt() );
		setMuted( false );
		quitWorkerSoon();
		return true;
	}

	return false;
}



bool ItlabAudioPlugin::setMuted( bool muted )
{
	return withEndpointVolume( [muted]( IAudioEndpointVolume* endpointVolume ) {
		BOOL current = FALSE;
		if( SUCCEEDED( endpointVolume->GetMute( &current ) ) && bool( current ) == muted )
		{
			return true;
		}
		return SUCCEEDED( endpointVolume->SetMute( muted ? TRUE : FALSE, nullptr ) );
	} );
}



bool ItlabAudioPlugin::setVolume( int percent )
{
	return withEndpointVolume( [percent]( IAudioEndpointVolume* endpointVolume ) {
		return SUCCEEDED( endpointVolume->SetMasterVolumeLevelScalar( float( qBound( 0, percent, 100 ) ) / 100.0f, nullptr ) );
	} );
}



bool ItlabAudioPlugin::readState( bool* muted, int* percent )
{
	return withEndpointVolume( [muted, percent]( IAudioEndpointVolume* endpointVolume ) {
		BOOL mute = FALSE;
		float level = 0;
		if( FAILED( endpointVolume->GetMute( &mute ) ) || FAILED( endpointVolume->GetMasterVolumeLevelScalar( &level ) ) )
		{
			return false;
		}
		*muted = mute;
		*percent = qRound( level * 100 );
		return true;
	} );
}



CommandLinePluginInterface::RunResult ItlabAudioPlugin::handle_status( const QStringList& arguments )
{
	Q_UNUSED(arguments)
	bool muted = false;
	int percent = 0;
	if( readState( &muted, &percent ) == false )
	{
		error( tr( "No audio output device found." ) );
		return Failed;
	}
	print( QStringLiteral("muted=%1 volume=%2").arg( muted ? 1 : 0 ).arg( percent ) );
	return Successful;
}



CommandLinePluginInterface::RunResult ItlabAudioPlugin::handle_mute( const QStringList& arguments )
{
	Q_UNUSED(arguments)
	return setMuted( true ) ? Successful : Failed;
}



CommandLinePluginInterface::RunResult ItlabAudioPlugin::handle_unmute( const QStringList& arguments )
{
	Q_UNUSED(arguments)
	return setMuted( false ) ? Successful : Failed;
}



CommandLinePluginInterface::RunResult ItlabAudioPlugin::handle_volume( const QStringList& arguments )
{
	bool ok = false;
	const auto percent = arguments.value( 0 ).toInt( &ok );
	if( ok == false )
	{
		return NotEnoughArguments;
	}
	return setVolume( percent ) ? Successful : Failed;
}



void ItlabAudioPlugin::quitWorkerSoon()
{
	// the worker is only needed while the computer has to stay muted
	QTimer::singleShot( 500, QCoreApplication::instance(), &QCoreApplication::quit );
}
