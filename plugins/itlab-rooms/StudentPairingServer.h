/*
 * StudentPairingServer.h - student computer side of pairing (ITLabControl Server)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QMap>
#include <QPointer>
#include <QTcpServer>

class QTcpSocket;
class ItlabRoomsPlugin;

// Runs in the ITLabControl Server (SYSTEM rights) of student computers. A teacher computer may ask
// for information ("hello") and offer its key ("offer"); the user at the computer has to confirm the
// offer (dialog shown by the worker in the user session) before the key is installed.
class StudentPairingServer : public QObject
{
	Q_OBJECT
public:
	explicit StudentPairingServer( ItlabRoomsPlugin* plugin );

	bool start();

	// answer of the user at the student computer (from the worker)
	void handleUserAnswer( const QString& requestId, bool accepted );

private:
	struct PendingOffer
	{
		QPointer<QTcpSocket> socket;
		QString teacher;
		QString room;
		QString keyName;
		QString publicKey;
	};

	void handleConnection();
	void handleMessage( QTcpSocket* socket );
	void handleOffer( QTcpSocket* socket, const QJsonObject& message );
	void reject( QTcpSocket* socket, const QString& reason );
	static QString macAddressOf( QTcpSocket* socket );

	ItlabRoomsPlugin* m_plugin;
	QTcpServer m_server;
	QMap<QString, PendingOffer> m_pendingOffers;

};
