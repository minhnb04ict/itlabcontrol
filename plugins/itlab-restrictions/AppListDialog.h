/*
 * AppListDialog.h - edit the list of allowed or blocked applications (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QObject>
#include <QStringList>

class QWidget;

class AppListDialog : public QObject
{
	Q_OBJECT
public:
	// returns false if the teacher cancelled
	static bool edit( QWidget* parent, bool allowList, QStringList& apps );
};
