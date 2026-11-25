/*
 * JoinTeacherDialog.h - student computer asks the teacher computer of its room to connect (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QDialog>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>

class QLabel;
class QProgressBar;
class QPushButton;

// Shown in the Configurator of a student computer (administrator rights): finds the teacher computer
// of the entered room (or uses the entered address), sends a join request and installs the key as soon
// as the teacher accepted it.
class JoinTeacherDialog : public QDialog
{
	Q_OBJECT
public:
	JoinTeacherDialog( const QString& room, const QString& teacherAddress, QWidget* parent );

	QString joinedRoom() const
	{
		return m_joinedRoom;
	}

private:
	void findTeacher();
	void handleDatagrams();
	void connectToTeacher( const QString& address, const QString& name );
	void handleReply();
	void fail( const QString& message );

	const QString m_room;
	QString m_teacherAddress;
	QString m_joinedRoom;

	QUdpSocket m_udpSocket;
	QTcpSocket m_tcpSocket;
	QTimer m_timer;
	bool m_connecting{false};

	QLabel* m_statusLabel;
	QProgressBar* m_progress;
	QPushButton* m_closeButton;

};
