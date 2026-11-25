/*
 * StudentPairingServer.cpp - student computer side of pairing (ITLabControl Server)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <QHostInfo>
#include <QNetworkInterface>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>

#include "FeatureMessage.h"
#include "FeatureWorkerManager.h"
#include "ItlabRoomsPlugin.h"
#include "KeyTools.h"
#include "PairingProtocol.h"
#include "PlatformUserFunctions.h"
#include "StudentPairingServer.h"
#include "VeyonServerInterface.h"


static constexpr int ConfirmationTimeout = 3 * 60 * 1000;


StudentPairingServer::StudentPairingServer( ItlabRoomsPlugin* plugin ) :
	QObject( plugin ),
	m_plugin( plugin )
{
	connect( &m_server, &QTcpServer::newConnection, this, &StudentPairingServer::handleConnection );
}



bool StudentPairingServer::start()
{
	if( m_server.listen( QHostAddress::Any, PairingProtocol::port() ) == false )
	{
		vWarning() << "could not listen on pairing port" << PairingProtocol::port() << m_server.errorString();
		return false;
	}
	return true;
}



void StudentPairingServer::handleConnection()
{
	while( m_server.hasPendingConnections() )
	{
		auto socket = m_server.nextPendingConnection();
		connect( socket, &QTcpSocket::readyRead, this, [this, socket]() { handleMessage( socket ); } );
		connect( socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater );
		// connections without a pending offer are closed after a while
		QTimer::singleShot( ConfirmationTimeout + 10000, socket, [socket]() { socket->disconnectFromHost(); } );
	}
}



void StudentPairingServer::handleMessage( QTcpSocket* socket )
{
	for( auto message = PairingProtocol::receive( socket ); message.isEmpty() == false;
		 message = PairingProtocol::receive( socket ) )
	{
		const auto type = PairingProtocol::type( message );
		if( type == QLatin1String("hello") )
		{
			PairingProtocol::send( socket, {
				{ QStringLiteral("type"), QStringLiteral("info") },
				{ QStringLiteral("hostName"), QHostInfo::localHostName() },
				{ QStringLiteral("mac"), macAddressOf( socket ) },
				{ QStringLiteral("room"), ItlabRoomsPlugin::configuredRoom() },
				{ QStringLiteral("paired"), KeyTools::hasTeacherKeys() },
			} );
		}
		else if( type == QLatin1String("offer") )
		{
			handleOffer( socket, message );
		}
	}
}



void StudentPairingServer::handleOffer( QTcpSocket* socket, const QJsonObject& message )
{
	const auto room = message.value( QStringLiteral("room") ).toString().simplified();
	const auto teacher = message.value( QStringLiteral("teacher") ).toString().simplified();

	// a computer which already belongs to a teacher can only be invited again for its own room
	if( KeyTools::hasTeacherKeys() &&
		ItlabRoomsPlugin::normalizedRoomName( room ) != ItlabRoomsPlugin::normalizedRoomName( ItlabRoomsPlugin::configuredRoom() ) )
	{
		reject( socket, tr( "This computer is already connected to the teacher computer of room \"%1\"." )
							.arg( ItlabRoomsPlugin::configuredRoom() ) );
		return;
	}

	if( m_pendingOffers.isEmpty() == false )
	{
		reject( socket, tr( "Another request is waiting for confirmation on this computer." ) );
		return;
	}

	if( VeyonCore::platform().userFunctions().isAnyUserLoggedOn() == false )
	{
		reject( socket, tr( "Nobody is logged on to this computer, so the request can not be confirmed." ) );
		return;
	}

	auto server = VeyonCore::instance()->findChild<VeyonServerInterface *>();
	if( server == nullptr )
	{
		reject( socket, QStringLiteral("server not available") );
		return;
	}

	const auto requestId = QUuid::createUuid().toString( QUuid::WithoutBraces );
	m_pendingOffers[requestId] = { socket, teacher, room,
								   message.value( QStringLiteral("keyName") ).toString(),
								   message.value( QStringLiteral("publicKey") ).toString() };

	// the user at this computer has to confirm (dialog shown by the worker in the user session)
	server->featureWorkerManager().sendMessageToUnmanagedSessionWorker(
		FeatureMessage{ m_plugin->featureUid(), ItlabRoomsPlugin::FeatureCommand::ConfirmPairing }
			.addArgument( ItlabRoomsPlugin::Argument::RequestId, requestId )
			.addArgument( ItlabRoomsPlugin::Argument::Teacher, teacher )
			.addArgument( ItlabRoomsPlugin::Argument::Room, room )
			.addArgument( ItlabRoomsPlugin::Argument::TeacherAddress, socket->peerAddress().toString() ) );

	QTimer::singleShot( ConfirmationTimeout, this, [this, requestId]() {
		if( m_pendingOffers.contains( requestId ) )
		{
			const auto offer = m_pendingOffers.take( requestId );
			reject( offer.socket, tr( "The request was not confirmed in time." ) );
		}
	} );
}



void StudentPairingServer::handleUserAnswer( const QString& requestId, bool accepted )
{
	if( m_pendingOffers.contains( requestId ) == false )
	{
		return;
	}

	const auto offer = m_pendingOffers.take( requestId );
	if( accepted == false )
	{
		reject( offer.socket, tr( "The request was declined on the student computer." ) );
		return;
	}

	QString error;
	if( KeyTools::applyTeacherKey( offer.keyName, offer.publicKey, offer.room, &error ) == false )
	{
		vWarning() << "could not apply teacher key:" << error;
		reject( offer.socket, tr( "The key of the teacher computer could not be installed: %1" ).arg( error ) );
		return;
	}

	vInfo() << "connected to teacher computer" << offer.teacher << "for room" << offer.room;

	PairingProtocol::send( offer.socket, {
		{ QStringLiteral("type"), QStringLiteral("accepted") },
		{ QStringLiteral("hostName"), QHostInfo::localHostName() },
		{ QStringLiteral("mac"), macAddressOf( offer.socket ) },
	} );
}



void StudentPairingServer::reject( QTcpSocket* socket, const QString& reason )
{
	PairingProtocol::send( socket, { { QStringLiteral("type"), QStringLiteral("rejected") },
									 { QStringLiteral("reason"), reason } } );
}



QString StudentPairingServer::macAddressOf( QTcpSocket* socket )
{
	if( socket == nullptr )
	{
		return {};
	}

	bool isIPv4 = false;
	const auto localIPv4 = socket->localAddress().toIPv4Address( &isIPv4 );
	const auto networkInterfaces = QNetworkInterface::allInterfaces();
	for( const auto& networkInterface : networkInterfaces )
	{
		const auto entries = networkInterface.addressEntries();
		for( const auto& entry : entries )
		{
			if( isIPv4 && entry.ip().toIPv4Address() == localIPv4 )
			{
				return networkInterface.hardwareAddress();
			}
		}
	}
	return {};
}
