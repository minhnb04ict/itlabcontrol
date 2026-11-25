/*
 * TeacherPairingService.h - teacher computer side of pairing (ITLabControl Teacher Console)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <functional>

#include <QPointer>
#include <QTcpServer>
#include <QUdpSocket>

class QDialog;
class QTcpSocket;
class QTreeWidget;
class ItlabRoomsPlugin;
class VeyonMasterInterface;

// Runs in the Teacher Console:
//  - answers student computers searching the teacher computer of their room (UDP)
//  - receives join requests of student computers; the teacher accepts them in a small window,
//    then the key is sent and the computer is added to the room
//  - invites discovered student computers (see RoomManagerDialog)
class TeacherPairingService : public QObject
{
	Q_OBJECT
public:
	struct StudentInfo
	{
		bool valid{false};
		QString hostName;
		QString mac;
		QString room;
		bool paired{false};
	};

	TeacherPairingService( ItlabRoomsPlugin* plugin, VeyonMasterInterface& master );

	void start();

	// asks a student computer for its name, room and pairing state
	void queryStudent( const QString& address, const std::function<void(const StudentInfo&)>& done );

	// sends the key to a student computer; the user there has to accept
	void inviteStudent( const QString& address, const QString& room,
						const std::function<void(bool accepted, const QString& hostName, const QString& mac,
												 const QString& reason)>& done );

	bool managesRoom( const QString& room ) const;

private:
	struct JoinRequest
	{
		QString id;
		QPointer<QTcpSocket> socket;
		QString hostName;
		QString address;
		QString mac;
		QString room;
		QString user;
	};

	void handleDatagrams();
	void handleConnection();
	void handleJoinMessage( QTcpSocket* socket );
	void showRequests();
	void updateRequestList();
	void acceptRequests( bool all );
	void rejectRequests();

	ItlabRoomsPlugin* m_plugin;
	VeyonMasterInterface& m_master;
	QUdpSocket m_udpSocket;
	QTcpServer m_tcpServer;
	QList<JoinRequest> m_requests;
	QPointer<QDialog> m_requestDialog;
	QTreeWidget* m_requestList{nullptr};

};
