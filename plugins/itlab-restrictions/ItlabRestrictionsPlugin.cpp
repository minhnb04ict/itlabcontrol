/*
 * ItlabRestrictionsPlugin.cpp - restrictions for student computers (ITLabControl)
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
 */

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>

#include "ComputerControlInterface.h"
#include "ItlabRestrictionsPlugin.h"
#include "VeyonConfiguration.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "WebsiteListDialog.h"


namespace {

qint64 now()
{
	return QDateTime::currentMSecsSinceEpoch();
}

int configuredMinutes( const QString& key, int defaultValue )
{
	const auto minutes = VeyonCore::config().value( key, QStringLiteral("ITLab"), defaultValue ).toInt();
	return minutes > 0 ? minutes : defaultValue;
}

}



ItlabRestrictionsPlugin::ItlabRestrictionsPlugin( QObject* parent ) :
	QObject( parent ),
	m_restrictionsFeature( QStringLiteral("ItlabRestrictions"),
						   Feature::Flag::Action | Feature::Flag::AllComponents,
						   Feature::Uid( "0e7b6d42-3c9a-4f85-b1d2-7a6e5f4c3b21" ),
						   Feature::Uid(),
						   tr( "Restrictions" ), {},
						   tr( "Restrict the student computers, e.g. block websites. "
							   "Restrictions are removed automatically when the Teacher Console is closed for a long time." ),
						   QStringLiteral(":/itlab-restrictions/itlab-restrictions.png") ),
	m_blockAllWebsitesFeature( QStringLiteral("ItlabBlockAllWebsites"),
							   Feature::Flag::Action | Feature::Flag::AllComponents,
							   Feature::Uid( "8a4c2e19-6b7d-4f3a-9e85-1c2b3d4e5f60" ),
							   m_restrictionsFeature.uid(),
							   tr( "Block all websites" ), {},
							   tr( "No website can be opened on the student computers." ) ),
	m_allowListedWebsitesFeature( QStringLiteral("ItlabAllowListedWebsites"),
								  Feature::Flag::Action | Feature::Flag::AllComponents,
								  Feature::Uid( "3d9f1b27-4a6c-4e8d-8b12-5f6a7c8d9e01" ),
								  m_restrictionsFeature.uid(),
								  tr( "Allow only listed websites..." ), {},
								  tr( "Only the websites in your list can be opened." ) ),
	m_blockListedWebsitesFeature( QStringLiteral("ItlabBlockListedWebsites"),
								  Feature::Flag::Action | Feature::Flag::AllComponents,
								  Feature::Uid( "6b2e8c40-1d3f-4a7b-9c56-2e3f4a5b6c72" ),
								  m_restrictionsFeature.uid(),
								  tr( "Block listed websites..." ), {},
								  tr( "The websites in your list can not be opened, all others can." ) ),
	m_unblockWebsitesFeature( QStringLiteral("ItlabUnblockWebsites"),
							  Feature::Flag::Action | Feature::Flag::AllComponents,
							  Feature::Uid( "9f3a5d61-2b4c-4d8e-a7f9-3b4c5d6e7f83" ),
							  m_restrictionsFeature.uid(),
							  tr( "Unblock websites" ), {},
							  tr( "All websites can be opened again." ) ),
	m_removeAllRestrictionsFeature( QStringLiteral("ItlabRemoveAllRestrictions"),
									Feature::Flag::Action | Feature::Flag::AllComponents,
									Feature::Uid( "1c7e9a83-5d2f-4b6c-8a1e-4c5d6e7f8a94" ),
									m_restrictionsFeature.uid(),
									tr( "Remove all restrictions" ), {},
									tr( "Remove every restriction from the student computers." ) ),
	m_features( { m_restrictionsFeature, m_blockAllWebsitesFeature, m_allowListedWebsitesFeature,
				  m_blockListedWebsitesFeature, m_unblockWebsitesFeature, m_removeAllRestrictionsFeature } ),
	m_commands( {
		{ QStringLiteral("status"), tr( "Show the restrictions which are active on this computer" ) },
		{ QStringLiteral("clear"), tr( "Remove all restrictions from this computer (emergency)" ) },
		{ QStringLiteral("web"), tr( "Apply a web filter on this computer: web blockall | web allow <websites> | web block <websites>" ) },
	} )
{
	m_heartbeatTimer.setInterval( HeartbeatInterval );
	connect( &m_heartbeatTimer, &QTimer::timeout, this, &ItlabRestrictionsPlugin::sendHeartbeats );

	m_expiryCheckTimer.setInterval( ExpiryCheckInterval );
	connect( &m_expiryCheckTimer, &QTimer::timeout, this, &ItlabRestrictionsPlugin::checkExpiry );

	// the plugin is loaded by every ITLabControl program - only the server enforces expiry
	QTimer::singleShot( 0, this, [this]() {
		if( VeyonCore::instance() && VeyonCore::component() == VeyonCore::Component::Server )
		{
			initServer();
		}
	} );
}



// ═════════════════════════════════════════════════════════════════════════════
//  MASTER
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabRestrictionsPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
											const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( hasFeature( feature.uid() ) == false || feature == m_restrictionsFeature )
	{
		return false;
	}

	// never restrict the teacher computer itself
	auto targets = computerControlInterfaces;
	targets.removeLocalHostInterfaces();
	if( targets.isEmpty() )
	{
		QMessageBox::information( master.mainWindow(), tr( "Restrictions" ),
								  tr( "Restrictions are never applied to this computer (the teacher computer)." ) );
		return true;
	}

	if( feature == m_allowListedWebsitesFeature || feature == m_blockListedWebsitesFeature )
	{
		const auto allow = feature == m_allowListedWebsitesFeature;
		auto config = master.userConfigurationObject();
		const auto configKey = allow ? QStringLiteral("AllowedWebsites") : QStringLiteral("BlockedWebsites");
		auto websites = config->value( configKey, QStringLiteral("ITLabRestrictions"), QStringList{} ).toStringList();

		if( WebsiteListDialog::edit( master.mainWindow(), allow, websites ) == false )
		{
			return true;
		}

		config->setValue( configKey, websites, QStringLiteral("ITLabRestrictions") );
		config->flushStore();

		if( RestrictionsBackend::normalizedWebsites( websites ).isEmpty() && allow == false )
		{
			return true; // nothing to block
		}

		controlFeature( feature.uid(), Operation::Start,
						{ { argToString( Argument::Websites ), websites } }, targets );
	}
	else
	{
		controlFeature( feature.uid(), Operation::Start, {}, targets );
	}

	addHeartbeatTargets( targets );

	return true;
}



bool ItlabRestrictionsPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
											  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( operation != Operation::Start )
	{
		return false;
	}

	FeatureMessage message{ m_restrictionsFeature.uid(), FeatureCommand::ApplyWebFilter };

	if( featureUid == m_blockAllWebsitesFeature.uid() )
	{
		message.addArgument( Argument::WebMode, RestrictionsBackend::webModeName( RestrictionsBackend::WebMode::BlockAll ) );
	}
	else if( featureUid == m_allowListedWebsitesFeature.uid() )
	{
		message.addArgument( Argument::WebMode, RestrictionsBackend::webModeName( RestrictionsBackend::WebMode::AllowListed ) )
				.addArgument( Argument::Websites, arguments.value( argToString( Argument::Websites ) ).toStringList() );
	}
	else if( featureUid == m_blockListedWebsitesFeature.uid() )
	{
		message.addArgument( Argument::WebMode, RestrictionsBackend::webModeName( RestrictionsBackend::WebMode::BlockListed ) )
				.addArgument( Argument::Websites, arguments.value( argToString( Argument::Websites ) ).toStringList() );
	}
	else if( featureUid == m_unblockWebsitesFeature.uid() )
	{
		message.setCommand( FeatureCommand::RemoveWebFilter );
	}
	else if( featureUid == m_removeAllRestrictionsFeature.uid() )
	{
		message.setCommand( FeatureCommand::RemoveAllRestrictions );
	}
	else
	{
		return false;
	}

	sendFeatureMessage( message, computerControlInterfaces );

	return true;
}



bool ItlabRestrictionsPlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
													const FeatureMessage& message )
{
	if( message.featureUid() == m_restrictionsFeature.uid() &&
		message.command<FeatureCommand>() == FeatureCommand::Status )
	{
		// restrictions are still active on this computer (e.g. after the Console was restarted):
		// keep it alive with heartbeats
		if( message.argument( Argument::Active ).toBool() )
		{
			addHeartbeatTargets( { computerControlInterface } );
		}
		return true;
	}

	return false;
}



void ItlabRestrictionsPlugin::addHeartbeatTargets( const ComputerControlInterfaceList& computerControlInterfaces )
{
	for( const auto& controlInterface : computerControlInterfaces )
	{
		const auto alreadyKnown = std::any_of( m_heartbeatTargets.cbegin(), m_heartbeatTargets.cend(),
											   [&controlInterface]( const QWeakPointer<ComputerControlInterface>& target ) {
			return target.toStrongRef() == controlInterface;
		} );
		if( alreadyKnown == false )
		{
			m_heartbeatTargets.append( controlInterface.toWeakRef() );
		}
	}

	if( m_heartbeatTimer.isActive() == false )
	{
		m_heartbeatTimer.start();
	}
}



void ItlabRestrictionsPlugin::sendHeartbeats()
{
	ComputerControlInterfaceList targets;
	for( auto it = m_heartbeatTargets.begin(); it != m_heartbeatTargets.end(); )
	{
		const auto controlInterface = it->toStrongRef();
		if( controlInterface.isNull() )
		{
			it = m_heartbeatTargets.erase( it );
			continue;
		}
		if( controlInterface->state() == ComputerControlInterface::State::Connected )
		{
			targets.append( controlInterface );
		}
		++it;
	}

	if( targets.isEmpty() == false )
	{
		sendFeatureMessage( FeatureMessage{ m_restrictionsFeature.uid(), FeatureCommand::Heartbeat }, targets );
	}
}



// ═════════════════════════════════════════════════════════════════════════════
//  SERVER
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabRestrictionsPlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
													const FeatureMessage& message )
{
	Q_UNUSED(server)
	Q_UNUSED(messageContext)

	if( message.featureUid() != m_restrictionsFeature.uid() )
	{
		return false;
	}

	QMutexLocker locker( &m_stateMutex );

	auto state = RestrictionsBackend::loadState();
	const auto timestamp = now();

	switch( message.command<FeatureCommand>() )
	{
	case FeatureCommand::ApplyWebFilter:
	{
		const auto mode = RestrictionsBackend::webModeFromName( message.argument( Argument::WebMode ).toString() );
		if( RestrictionsBackend::applyWebFilter( mode, message.argument( Argument::Websites ).toStringList(), state ) == false )
		{
			vWarning() << "web filter could not be applied completely";
		}
		state[QStringLiteral("since")] = timestamp;
		state[QStringLiteral("expires")] = timestamp + qint64( configuredMinutes( QStringLiteral("RestrictionMaxDuration"), 240 ) ) * 60000;
		state[QStringLiteral("lastHeartbeat")] = timestamp;
		vInfo() << "web filter applied:" << RestrictionsBackend::webModeName( mode );
		break;
	}

	case FeatureCommand::RemoveWebFilter:
		RestrictionsBackend::removeWebFilter( state );
		if( RestrictionsBackend::isAnyRestrictionActive( state ) == false )
		{
			RestrictionsBackend::removeAllRestrictions( state );
		}
		vInfo() << "web filter removed";
		break;

	case FeatureCommand::RemoveAllRestrictions:
		RestrictionsBackend::removeAllRestrictions( state );
		vInfo() << "all restrictions removed";
		break;

	case FeatureCommand::Heartbeat:
		// writing the state file once a minute is enough
		if( RestrictionsBackend::isAnyRestrictionActive( state ) == false ||
			timestamp - m_lastHeartbeatWrite < HeartbeatInterval / 2 )
		{
			return true;
		}
		state[QStringLiteral("lastHeartbeat")] = timestamp;
		m_lastHeartbeatWrite = timestamp;
		break;

	default:
		return true;
	}

	RestrictionsBackend::saveState( state );
	updateActiveState( state );

	return true;
}



void ItlabRestrictionsPlugin::sendAsyncFeatureMessages( VeyonServerInterface& server, const MessageContext& messageContext )
{
	static const char* statusSentProperty = "itlabRestrictionStatusSent";

	auto ioDevice = messageContext.ioDevice();
	if( ioDevice == nullptr || ioDevice->property( statusSentProperty ).toBool() )
	{
		return;
	}
	ioDevice->setProperty( statusSentProperty, true );

	bool active = false;
	{
		QMutexLocker locker( &m_stateMutex );
		active = m_active;
	}

	if( active )
	{
		server.sendFeatureMessageReply( messageContext,
										FeatureMessage{ m_restrictionsFeature.uid(), FeatureCommand::Status }
											.addArgument( Argument::Active, true ) );
	}
}



bool ItlabRestrictionsPlugin::isFeatureActive( VeyonServerInterface& server, Feature::Uid featureUid ) const
{
	Q_UNUSED(server)

	QMutexLocker locker( &m_stateMutex );

	if( featureUid == m_restrictionsFeature.uid() )
	{
		return m_active;
	}
	if( featureUid == m_blockAllWebsitesFeature.uid() )
	{
		return m_webMode == RestrictionsBackend::WebMode::BlockAll;
	}
	if( featureUid == m_allowListedWebsitesFeature.uid() )
	{
		return m_webMode == RestrictionsBackend::WebMode::AllowListed;
	}
	if( featureUid == m_blockListedWebsitesFeature.uid() )
	{
		return m_webMode == RestrictionsBackend::WebMode::BlockListed;
	}

	return false;
}



void ItlabRestrictionsPlugin::initServer()
{
	// restrictions of a previous session (e.g. before a reboot) may have expired meanwhile
	checkExpiry();
	m_expiryCheckTimer.start();
}



void ItlabRestrictionsPlugin::checkExpiry()
{
	QMutexLocker locker( &m_stateMutex );

	auto state = RestrictionsBackend::loadState();

	if( RestrictionsBackend::isAnyRestrictionActive( state ) )
	{
		const auto timestamp = now();
		const auto heartbeatTimeout = qint64( configuredMinutes( QStringLiteral("RestrictionHeartbeatTimeout"), 15 ) ) * 60000;
		const auto lastHeartbeat = state.value( QStringLiteral("lastHeartbeat") ).toVariant().toLongLong();
		const auto expires = state.value( QStringLiteral("expires") ).toVariant().toLongLong();

		QString reason;
		if( timestamp - lastHeartbeat > heartbeatTimeout )
		{
			reason = QStringLiteral("no heartbeat from the Teacher Console");
		}
		else if( expires > 0 && timestamp > expires )
		{
			reason = QStringLiteral("maximum duration reached");
		}

		if( reason.isEmpty() == false )
		{
			vWarning() << "removing all restrictions:" << reason;
			RestrictionsBackend::removeAllRestrictions( state );
			RestrictionsBackend::saveState( state );
		}
	}
	else if( state.contains( QStringLiteral("policyBackups") ) )
	{
		// an earlier removal did not succeed completely - try again
		RestrictionsBackend::removeAllRestrictions( state );
		RestrictionsBackend::saveState( state );
	}

	updateActiveState( state );
}



void ItlabRestrictionsPlugin::updateActiveState( const QJsonObject& state )
{
	m_active = RestrictionsBackend::isAnyRestrictionActive( state );
	m_webMode = RestrictionsBackend::activeWebMode( state );
}



// ═════════════════════════════════════════════════════════════════════════════
//  COMMAND LINE
// ═════════════════════════════════════════════════════════════════════════════

QStringList ItlabRestrictionsPlugin::commands() const
{
	return m_commands.keys();
}



QString ItlabRestrictionsPlugin::commandHelp( const QString& command ) const
{
	return m_commands.value( command );
}



CommandLinePluginInterface::RunResult ItlabRestrictionsPlugin::handle_status( const QStringList& arguments )
{
	Q_UNUSED(arguments)

	const auto state = RestrictionsBackend::loadState();
	if( RestrictionsBackend::isAnyRestrictionActive( state ) == false )
	{
		print( tr( "No restrictions are active on this computer." ) );
		return Successful;
	}

	const auto web = state.value( QStringLiteral("web") ).toObject();
	print( tr( "Web filter: %1" ).arg( web.value( QStringLiteral("mode") ).toString() ) );
	const auto websites = web.value( QStringLiteral("websites") ).toArray();
	for( const auto& website : websites )
	{
		print( QStringLiteral("    ") + website.toString() );
	}
	print( tr( "Last heartbeat: %1" ).arg( QDateTime::fromMSecsSinceEpoch(
		state.value( QStringLiteral("lastHeartbeat") ).toVariant().toLongLong() ).toString( Qt::ISODate ) ) );
	print( tr( "Expires: %1" ).arg( QDateTime::fromMSecsSinceEpoch(
		state.value( QStringLiteral("expires") ).toVariant().toLongLong() ).toString( Qt::ISODate ) ) );

	return Successful;
}



CommandLinePluginInterface::RunResult ItlabRestrictionsPlugin::handle_web( const QStringList& arguments )
{
	static const QMap<QString, RestrictionsBackend::WebMode> modes{
		{ QStringLiteral("blockall"), RestrictionsBackend::WebMode::BlockAll },
		{ QStringLiteral("allow"), RestrictionsBackend::WebMode::AllowListed },
		{ QStringLiteral("block"), RestrictionsBackend::WebMode::BlockListed },
	};

	const auto mode = modes.value( arguments.value( 0 ).toLower(), RestrictionsBackend::WebMode::None );
	if( mode == RestrictionsBackend::WebMode::None )
	{
		return NotEnoughArguments;
	}

	// the restriction expires like one applied from the Teacher Console, since no heartbeats will follow
	auto state = RestrictionsBackend::loadState();
	const auto success = RestrictionsBackend::applyWebFilter( mode, arguments.mid( 1 ), state );
	const auto timestamp = QDateTime::currentMSecsSinceEpoch();
	state[QStringLiteral("since")] = timestamp;
	state[QStringLiteral("expires")] = timestamp + qint64( configuredMinutes( QStringLiteral("RestrictionMaxDuration"), 240 ) ) * 60000;
	state[QStringLiteral("lastHeartbeat")] = timestamp;
	RestrictionsBackend::saveState( state );

	if( success == false )
	{
		error( tr( "The web filter could not be applied completely. Please run this command as administrator." ) );
		return Failed;
	}

	info( tr( "Web filter applied. It is removed automatically when no Teacher Console keeps it alive." ) );
	return Successful;
}



CommandLinePluginInterface::RunResult ItlabRestrictionsPlugin::handle_clear( const QStringList& arguments )
{
	Q_UNUSED(arguments)

	auto state = RestrictionsBackend::loadState();
	const auto success = RestrictionsBackend::removeAllRestrictions( state );
	RestrictionsBackend::saveState( state );

	if( success == false )
	{
		error( tr( "Not all restrictions could be removed. Please run this command as administrator." ) );
		return Failed;
	}

	info( tr( "All restrictions have been removed." ) );
	return Successful;
}
