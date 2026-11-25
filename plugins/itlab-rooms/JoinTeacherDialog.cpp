/*
 * JoinTeacherDialog.cpp - student computer asks the teacher computer of its room to connect (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <QHostInfo>
#include <QLabel>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include "JoinTeacherDialog.h"
#include "KeyTools.h"
#include "PairingProtocol.h"


static constexpr int JoinSearchTimeout = 3000;
static constexpr int JoinConfirmationTimeout = 5 * 60 * 1000;


JoinTeacherDialog::JoinTeacherDialog( const QString& room, const QString& teacherAddress, QWidget* parent ) :
	QDialog( parent ),
	m_room( room.simplified() ),
	m_teacherAddress( teacherAddress.trimmed() ),
	m_statusLabel( new QLabel( this ) ),
	m_progress( new QProgressBar( this ) ),
	m_closeButton( new QPushButton( tr( "Cancel" ), this ) )
{
	setWindowTitle( tr( "Connect to teacher computer" ) );
	resize( 480, 160 );

	m_statusLabel->setWordWrap( true );
	m_progress->setRange( 0, 0 );

	auto layout = new QVBoxLayout( this );
	layout->addWidget( m_statusLabel );
	layout->addWidget( m_progress );
	layout->addWidget( m_closeButton, 0, Qt::AlignRight );

	connect( m_closeButton, &QPushButton::clicked, this, [this]() {
		m_joinedRoom.isEmpty() ? reject() : accept();
	} );
	connect( &m_udpSocket, &QUdpSocket::readyRead, this, &JoinTeacherDialog::handleDatagrams );
	connect( &m_tcpSocket, &QTcpSocket::readyRead, this, &JoinTeacherDialog::handleReply );
	connect( &m_tcpSocket, &QTcpSocket::errorOccurred, this, [this]() {
		if( m_joinedRoom.isEmpty() )
		{
			fail( tr( "The connection to the teacher computer failed (%1). Please check that the Teacher Console "
					  "is open on the teacher computer." ).arg( m_tcpSocket.errorString() ) );
		}
	} );
	connect( &m_tcpSocket, &QTcpSocket::connected, this, [this]() {
		PairingProtocol::send( &m_tcpSocket, {
			{ QStringLiteral("type"), QStringLiteral("join") },
			{ QStringLiteral("hostName"), QHostInfo::localHostName() },
			{ QStringLiteral("room"), m_room },
			{ QStringLiteral("user"), qEnvironmentVariable( "USERNAME" ) },
		} );
		m_statusLabel->setText( tr( "Waiting until the teacher accepts the request in the Teacher Console..." ) );
	} );

	m_timer.setSingleShot( true );

	if( m_teacherAddress.isEmpty() )
	{
		findTeacher();
	}
	else
	{
		connectToTeacher( m_teacherAddress, m_teacherAddress );
	}
}



void JoinTeacherDialog::findTeacher()
{
	m_statusLabel->setText( tr( "Searching the teacher computer of room \"%1\" in the network..." ).arg( m_room ) );

	m_udpSocket.bind( QHostAddress::AnyIPv4, 0 );
	const auto request = QJsonDocument( QJsonObject{
		{ QStringLiteral("type"), QStringLiteral("findTeacher") },
		{ QStringLiteral("room"), m_room },
	} ).toJson( QJsonDocument::Compact );

	m_udpSocket.writeDatagram( request, QHostAddress::Broadcast, PairingProtocol::port() );
	const auto networkInterfaces = QNetworkInterface::allInterfaces();
	for( const auto& networkInterface : networkInterfaces )
	{
		const auto entries = networkInterface.addressEntries();
		for( const auto& entry : entries )
		{
			if( entry.broadcast().isNull() == false )
			{
				m_udpSocket.writeDatagram( request, entry.broadcast(), PairingProtocol::port() );
			}
		}
	}

	disconnect( &m_timer, nullptr, this, nullptr );
	connect( &m_timer, &QTimer::timeout, this, [this]() {
		if( m_connecting == false )
		{
			fail( tr( "No teacher computer of room \"%1\" was found. Please open the Teacher Console on the teacher "
					  "computer, check that the room names are the same, or enter the address of the teacher computer "
					  "(needed if it is in another network)." ).arg( m_room ) );
		}
	} );
	m_timer.start( JoinSearchTimeout );
}



void JoinTeacherDialog::handleDatagrams()
{
	while( m_udpSocket.hasPendingDatagrams() )
	{
		const auto datagram = m_udpSocket.receiveDatagram( 4096 );
		const auto message = QJsonDocument::fromJson( datagram.data() ).object();
		if( m_connecting == false && PairingProtocol::type( message ) == QLatin1String("teacher") )
		{
			bool isIPv4 = false;
			const auto ipv4 = datagram.senderAddress().toIPv4Address( &isIPv4 );
			const auto address = isIPv4 ? QHostAddress( ipv4 ).toString() : datagram.senderAddress().toString();
			connectToTeacher( address, message.value( QStringLiteral("name") ).toString() );
		}
	}
}



void JoinTeacherDialog::connectToTeacher( const QString& address, const QString& name )
{
	m_connecting = true;
	m_statusLabel->setText( tr( "Connecting to the teacher computer %1..." ).arg( name ) );
	m_tcpSocket.connectToHost( address, PairingProtocol::port() );

	disconnect( &m_timer, nullptr, this, nullptr );
	connect( &m_timer, &QTimer::timeout, this, [this]() {
		fail( tr( "The teacher did not accept the request in time." ) );
	} );
	m_timer.start( JoinConfirmationTimeout );
}



void JoinTeacherDialog::handleReply()
{
	const auto message = PairingProtocol::receive( &m_tcpSocket );
	const auto type = PairingProtocol::type( message );

	if( type == QLatin1String("rejected") )
	{
		fail( tr( "The teacher declined the request." ) );
		return;
	}

	if( type != QLatin1String("accepted") )
	{
		return;
	}

	m_timer.stop();
	const auto room = message.value( QStringLiteral("room") ).toString();
	QString error;
	if( KeyTools::applyTeacherKey( message.value( QStringLiteral("keyName") ).toString(),
								   message.value( QStringLiteral("publicKey") ).toString(), room, &error ) == false )
	{
		fail( tr( "The key of the teacher computer could not be installed: %1" ).arg( error ) );
		return;
	}

	// the service uses the new key and authentication method after a restart
	KeyTools::runCli( { QStringLiteral("service"), QStringLiteral("restart") } );

	m_joinedRoom = room;
	m_tcpSocket.disconnectFromHost();
	m_progress->setRange( 0, 1 );
	m_progress->setValue( 1 );
	m_statusLabel->setText( tr( "Done. This computer is connected to the teacher computer %1 and belongs to room \"%2\"." )
							.arg( message.value( QStringLiteral("teacher") ).toString(), room ) );
	m_closeButton->setText( tr( "Close" ) );
}



void JoinTeacherDialog::fail( const QString& message )
{
	m_timer.stop();
	m_tcpSocket.abort();
	m_progress->setRange( 0, 1 );
	m_progress->setValue( 0 );
	m_statusLabel->setText( message );
	m_closeButton->setText( tr( "Close" ) );
}
