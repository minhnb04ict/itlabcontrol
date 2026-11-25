/*
 * PairingProtocol.h - connect student computers to a teacher computer without copying key files (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 *  Port: ITLabControl server port + 400 (14500 by default), TCP and UDP, one JSON object per line.
 *
 *  A) Student asks to join a room (Configurator of the student computer, administrator):
 *     UDP broadcast  {"type":"findTeacher","room":R}             -> Teacher Console answers
 *                    {"type":"teacher","room":R,"name":N}           if it manages room R
 *     TCP to teacher {"type":"join","hostName","mac","room","user"}
 *                    <- {"type":"accepted","keyName","publicKey","room","teacher"} after the teacher accepted
 *                    <- {"type":"rejected"}
 *
 *  B) Teacher invites a discovered student computer (Teacher Console, "Discover computers"):
 *     TCP to student {"type":"hello"}          <- {"type":"info","hostName","mac","room","paired"}
 *                    {"type":"offer","teacher","room","keyName","publicKey"}
 *                    <- {"type":"accepted","hostName","mac"} after the user at the student computer accepted
 *                    <- {"type":"rejected","reason"}
 *
 *  Security: a student computer only accepts an offer when it is not connected to a teacher yet or
 *  when the offer is for its own room, and only after the user at the computer confirmed it.
 *  Joining (A) needs the Configurator, i.e. administrator rights on the student computer, and the
 *  confirmation of the teacher.
 */

#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>

#include "VeyonConfiguration.h"

namespace PairingProtocol {

inline quint16 port()
{
	return quint16( VeyonCore::config().veyonServerPort() + 400 );
}

inline void send( QTcpSocket* socket, const QJsonObject& message )
{
	if( socket && socket->state() == QAbstractSocket::ConnectedState )
	{
		socket->write( QJsonDocument( message ).toJson( QJsonDocument::Compact ) + '\n' );
		socket->flush();
	}
}

// returns the next complete message or an empty object
inline QJsonObject receive( QTcpSocket* socket )
{
	if( socket == nullptr || socket->canReadLine() == false )
	{
		return {};
	}
	const auto line = socket->readLine( 64 * 1024 );
	return QJsonDocument::fromJson( line ).object();
}

inline QString type( const QJsonObject& message )
{
	return message.value( QStringLiteral("type") ).toString();
}

}
