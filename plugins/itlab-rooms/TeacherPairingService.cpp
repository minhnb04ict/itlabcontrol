/*
 * TeacherPairingService.cpp - teacher computer side of pairing (ITLabControl Teacher Console)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHostInfo>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkDatagram>
#include <QPushButton>
#include <QSharedPointer>
#include <QTcpSocket>
#include <QTimer>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>

#include "ItlabRoomsPlugin.h"
#include "KeyTools.h"
#include "NetworkObject.h"
#include "PairingProtocol.h"
#include "TeacherPairingService.h"
#include "VeyonMasterInterface.h"


namespace {

QString ipv4String( const QHostAddress& address )
{
	bool isIPv4 = false;
	const auto ipv4 = address.toIPv4Address( &isIPv4 );
	return isIPv4 ? QHostAddress( ipv4 ).toString() : address.toString();
}

}



TeacherPairingService::TeacherPairingService( ItlabRoomsPlugin* plugin, VeyonMasterInterface& master ) :
	QObject( plugin ),
	m_plugin( plugin ),
	m_master( master )
{
	connect( &m_udpSocket, &QUdpSocket::readyRead, this, &TeacherPairingService::handleDatagrams );
	connect( &m_tcpServer, &QTcpServer::newConnection, this, &TeacherPairingService::handleConnection );
}



void TeacherPairingService::start()
{
	if( m_udpSocket.bind( QHostAddress::AnyIPv4, PairingProtocol::port(),
						  QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint ) == false )
	{
		vWarning() << "could not bind pairing UDP port" << m_udpSocket.errorString();
	}
	if( m_tcpServer.listen( QHostAddress::Any, PairingProtocol::port() ) == false )
	{
		vWarning() << "could not listen on pairing port" << m_tcpServer.errorString();
	}
}



bool TeacherPairingService::managesRoom( const QString& room ) const
{
	const auto normalized = ItlabRoomsPlugin::normalizedRoomName( room );
	if( normalized.isEmpty() )
	{
		return false;
	}
	if( ItlabRoomsPlugin::normalizedRoomName( ItlabRoomsPlugin::configuredRoom() ) == normalized )
	{
		return true;
	}

	// any room of the Teacher Console
	const auto objects = VeyonCore::config().value( QStringLiteral("NetworkObjects"), QStringLiteral("BuiltinDirectory"),
													QJsonArray() ).toJsonArray();
	for( const auto& value : objects )
	{
		const auto object = value.toObject();
		if( object.value( QStringLiteral("Type") ).toInt() == int( NetworkObject::Type::Location ) &&
			ItlabRoomsPlugin::normalizedRoomName( object.value( QStringLiteral("Name") ).toString() ) == normalized )
		{
			return true;
		}
	}
	return false;
}



void TeacherPairingService::handleDatagrams()
{
	while( m_udpSocket.hasPendingDatagrams() )
	{
		const auto datagram = m_udpSocket.receiveDatagram( 4096 );
		const auto message = QJsonDocument::fromJson( datagram.data() ).object();
		if( PairingProtocol::type( message ) != QLatin1String("findTeacher") )
		{
			continue;
		}

		const auto room = message.value( QStringLiteral("room") ).toString();
		if( managesRoom( room ) )
		{
			const auto reply = QJsonDocument( QJsonObject{
				{ QStringLiteral("type"), QStringLiteral("teacher") },
				{ QStringLiteral("room"), room },
				{ QStringLiteral("name"), QHostInfo::localHostName() },
			} ).toJson( QJsonDocument::Compact );
			m_udpSocket.writeDatagram( reply, datagram.senderAddress(), quint16( datagram.senderPort() ) );
		}
	}
}



void TeacherPairingService::handleConnection()
{
	while( m_tcpServer.hasPendingConnections() )
	{
		auto socket = m_tcpServer.nextPendingConnection();
		connect( socket, &QTcpSocket::readyRead, this, [this, socket]() { handleJoinMessage( socket ); } );
		connect( socket, &QTcpSocket::disconnected, this, [this, socket]() {
			// the student gave up waiting - remove the request
			for( int i = 0; i < m_requests.size(); ++i )
			{
				if( m_requests[i].socket == socket )
				{
					m_requests.removeAt( i );
					updateRequestList();
					break;
				}
			}
			socket->deleteLater();
		} );
	}
}



void TeacherPairingService::handleJoinMessage( QTcpSocket* socket )
{
	for( auto message = PairingProtocol::receive( socket ); message.isEmpty() == false;
		 message = PairingProtocol::receive( socket ) )
	{
		if( PairingProtocol::type( message ) != QLatin1String("join") )
		{
			continue;
		}

		JoinRequest request;
		request.id = QUuid::createUuid().toString( QUuid::WithoutBraces );
		request.socket = socket;
		request.hostName = message.value( QStringLiteral("hostName") ).toString().simplified();
		request.address = ipv4String( socket->peerAddress() );
		request.mac = message.value( QStringLiteral("mac") ).toString();
		request.room = message.value( QStringLiteral("room") ).toString().simplified();
		request.user = message.value( QStringLiteral("user") ).toString();
		if( request.hostName.isEmpty() || request.room.isEmpty() )
		{
			socket->disconnectFromHost();
			return;
		}

		m_requests.append( request );
		showRequests();
	}
}



void TeacherPairingService::showRequests()
{
	if( m_requestDialog.isNull() )
	{
		auto dialog = new QDialog( m_master.mainWindow() );
		dialog->setAttribute( Qt::WA_DeleteOnClose );
		dialog->setWindowTitle( tr( "Computers asking to join a room" ) );
		dialog->resize( 640, 360 );

		auto description = new QLabel( tr( "These student computers ask to be connected to this teacher computer. "
											"Accept them to send the authentication key and add them to their room." ), dialog );
		description->setWordWrap( true );

		m_requestList = new QTreeWidget( dialog );
		m_requestList->setRootIsDecorated( false );
		m_requestList->setSelectionMode( QAbstractItemView::ExtendedSelection );
		m_requestList->setHeaderLabels( { tr( "Computer" ), tr( "IP address" ), tr( "Room" ), tr( "User" ) } );

		auto buttons = new QDialogButtonBox( dialog );
		auto acceptButton = buttons->addButton( tr( "Accept selected" ), QDialogButtonBox::ActionRole );
		auto acceptAllButton = buttons->addButton( tr( "Accept all" ), QDialogButtonBox::ActionRole );
		auto rejectButton = buttons->addButton( tr( "Reject selected" ), QDialogButtonBox::ActionRole );
		buttons->addButton( QDialogButtonBox::Close );
		connect( acceptButton, &QPushButton::clicked, this, [this]() { acceptRequests( false ); } );
		connect( acceptAllButton, &QPushButton::clicked, this, [this]() { acceptRequests( true ); } );
		connect( rejectButton, &QPushButton::clicked, this, &TeacherPairingService::rejectRequests );
		connect( buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close );

		auto layout = new QVBoxLayout( dialog );
		layout->addWidget( description );
		layout->addWidget( m_requestList, 1 );
		layout->addWidget( buttons );

		m_requestDialog = dialog;
	}

	updateRequestList();
	m_requestDialog->show();
	m_requestDialog->raise();
	m_requestDialog->activateWindow();
}



void TeacherPairingService::updateRequestList()
{
	if( m_requestDialog.isNull() || m_requestList == nullptr )
	{
		return;
	}

	m_requestList->clear();
	for( const auto& request : std::as_const( m_requests ) )
	{
		auto item = new QTreeWidgetItem( m_requestList, { request.hostName, request.address, request.room, request.user } );
		item->setData( 0, Qt::UserRole, request.id );
	}
	for( int i = 0; i < m_requestList->columnCount(); ++i )
	{
		m_requestList->resizeColumnToContents( i );
	}
}



void TeacherPairingService::acceptRequests( bool all )
{
	QStringList ids;
	for( int i = 0; i < m_requestList->topLevelItemCount(); ++i )
	{
		const auto item = m_requestList->topLevelItem( i );
		if( all || item->isSelected() )
		{
			ids.append( item->data( 0, Qt::UserRole ).toString() );
		}
	}
	if( ids.isEmpty() )
	{
		return;
	}

	QString keyName;
	QString publicKey;
	if( KeyTools::teacherPublicKey( &keyName, &publicKey ) == false )
	{
		QMessageBox::critical( m_requestDialog, tr( "Connect computers" ),
							   tr( "This teacher computer has no authentication key. Please create one in the "
								   "ITLabControl Configurator (page \"Room\")." ) );
		return;
	}

	for( int i = m_requests.size() - 1; i >= 0; --i )
	{
		const auto request = m_requests[i];
		if( ids.contains( request.id ) == false )
		{
			continue;
		}
		m_requests.removeAt( i );

		PairingProtocol::send( request.socket, {
			{ QStringLiteral("type"), QStringLiteral("accepted") },
			{ QStringLiteral("keyName"), keyName },
			{ QStringLiteral("publicKey"), publicKey },
			{ QStringLiteral("room"), request.room },
			{ QStringLiteral("teacher"), QHostInfo::localHostName() },
		} );

		m_plugin->addPairedComputer( request.hostName, request.address, request.mac, request.room );
	}

	updateRequestList();
	if( m_requests.isEmpty() && m_requestDialog )
	{
		m_requestDialog->close();
	}
}



void TeacherPairingService::rejectRequests()
{
	for( int i = m_requests.size() - 1; i >= 0; --i )
	{
		bool selected = false;
		for( int j = 0; j < m_requestList->topLevelItemCount(); ++j )
		{
			const auto item = m_requestList->topLevelItem( j );
			if( item->isSelected() && item->data( 0, Qt::UserRole ).toString() == m_requests[i].id )
			{
				selected = true;
			}
		}
		if( selected )
		{
			PairingProtocol::send( m_requests[i].socket, { { QStringLiteral("type"), QStringLiteral("rejected") } } );
			m_requests.removeAt( i );
		}
	}
	updateRequestList();
}



void TeacherPairingService::queryStudent( const QString& address, const std::function<void(const StudentInfo&)>& done )
{
	auto socket = new QTcpSocket( this );
	auto finished = QSharedPointer<bool>::create( false );

	const auto finish = [socket, finished, done]( const StudentInfo& info ) {
		if( *finished == false )
		{
			*finished = true;
			done( info );
			socket->abort();
			socket->deleteLater();
		}
	};

	connect( socket, &QTcpSocket::connected, socket, [socket]() {
		PairingProtocol::send( socket, { { QStringLiteral("type"), QStringLiteral("hello") } } );
	} );
	connect( socket, &QTcpSocket::readyRead, socket, [socket, finish]() {
		const auto message = PairingProtocol::receive( socket );
		if( PairingProtocol::type( message ) == QLatin1String("info") )
		{
			StudentInfo info;
			info.valid = true;
			info.hostName = message.value( QStringLiteral("hostName") ).toString();
			info.mac = message.value( QStringLiteral("mac") ).toString();
			info.room = message.value( QStringLiteral("room") ).toString();
			info.paired = message.value( QStringLiteral("paired") ).toBool();
			finish( info );
		}
	} );
	connect( socket, &QTcpSocket::errorOccurred, socket, [finish]() { finish( {} ); } );
	QTimer::singleShot( 5000, socket, [finish]() { finish( {} ); } );

	socket->connectToHost( address, PairingProtocol::port() );
}



void TeacherPairingService::inviteStudent( const QString& address, const QString& room,
										   const std::function<void(bool, const QString&, const QString&, const QString&)>& done )
{
	QString keyName;
	QString publicKey;
	if( KeyTools::teacherPublicKey( &keyName, &publicKey ) == false )
	{
		done( false, {}, {}, tr( "This teacher computer has no authentication key. Please create one in the "
								 "ITLabControl Configurator (page \"Room\")." ) );
		return;
	}

	auto socket = new QTcpSocket( this );
	auto finished = QSharedPointer<bool>::create( false );

	const auto finish = [socket, finished, done]( bool accepted, const QString& hostName, const QString& mac,
												  const QString& reason ) {
		if( *finished == false )
		{
			*finished = true;
			done( accepted, hostName, mac, reason );
			socket->disconnectFromHost();
			socket->deleteLater();
		}
	};

	connect( socket, &QTcpSocket::connected, socket, [socket, room, keyName, publicKey]() {
		PairingProtocol::send( socket, {
			{ QStringLiteral("type"), QStringLiteral("offer") },
			{ QStringLiteral("teacher"), QHostInfo::localHostName() },
			{ QStringLiteral("room"), room },
			{ QStringLiteral("keyName"), keyName },
			{ QStringLiteral("publicKey"), publicKey },
		} );
	} );
	connect( socket, &QTcpSocket::readyRead, socket, [socket, finish]() {
		const auto message = PairingProtocol::receive( socket );
		const auto type = PairingProtocol::type( message );
		if( type == QLatin1String("accepted") )
		{
			finish( true, message.value( QStringLiteral("hostName") ).toString(),
					message.value( QStringLiteral("mac") ).toString(), {} );
		}
		else if( type == QLatin1String("rejected") )
		{
			finish( false, {}, {}, message.value( QStringLiteral("reason") ).toString() );
		}
	} );
	connect( socket, &QTcpSocket::errorOccurred, socket, [finish]() {
		finish( false, {}, {}, tr( "The student computer is not reachable." ) );
	} );
	// the user at the student computer has up to 3 minutes to confirm
	QTimer::singleShot( 200 * 1000, socket, [finish]() { finish( false, {}, {}, tr( "No answer from the student computer." ) ); } );

	socket->connectToHost( address, PairingProtocol::port() );
}
