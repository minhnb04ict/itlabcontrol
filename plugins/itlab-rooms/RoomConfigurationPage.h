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

private:
	QLineEdit* m_roomEdit;

};
