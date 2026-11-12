/*
 * RoomConfigurationPage.h - trang "Phòng máy" trong Configurator
 *
 * Copyright (c) 2026 ITLabControl contributors
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ConfigurationPage.h"

class QLabel;
class QLineEdit;

// Cho phép sửa tên phòng của máy này (lưu ở ITLab/Room) ngay trên máy học sinh
class RoomConfigurationPage : public ConfigurationPage
{
	Q_OBJECT
public:
	explicit RoomConfigurationPage( QWidget* parent = nullptr );
	~RoomConfigurationPage() override = default;

	void resetWidgets() override;
	void connectWidgetsToProperties() override;
	void applyConfiguration() override;

	// "Phòng 201" -> "phong-201" (authentication key names only allow [A-Za-z0-9_-])
	static QString keyNameForRoom( const QString& room );

private:
	// ITLabControl: one authentication key pair per room, so that the teacher computer of
	// room A can not control the student computers of room B
	void updateKeyStatus();
	void createRoomKey();
	void restrictToKeys();

	static QStringList keyNames( bool privateKeys );
	bool runCli( const QStringList& arguments, QString* output = nullptr );

	// ITLabControl: school logo and message on the locked screen
	static QString lockScreenImagePath();
	void updateLockScreenPreview();
	void chooseLockScreenImage();
	void resetLockScreenImage();

	QLineEdit* m_roomEdit;
	QLabel* m_keyStatusLabel;
	QLabel* m_lockScreenPreview;
	QLineEdit* m_lockScreenTextEdit;

};
