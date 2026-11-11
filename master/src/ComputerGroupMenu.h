/*
 * ComputerGroupMenu.h - menus for working with computer groups (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include <QObject>

#include "ComputerControlInterface.h"

class QMenu;
class QToolButton;
class QWidget;
class VeyonMaster;

class ComputerGroupMenu : public QObject
{
	Q_OBJECT
public:
	// status bar button: "Group: All computers" with a menu for choosing the active group
	static QToolButton* createStatusBarButton( VeyonMaster& master, QWidget* parent );

	// "Groups" sub menu for the context menu of the selected computers
	static void addContextMenu( QMenu* menu, VeyonMaster& master, QWidget* parent,
								const ComputerControlInterfaceList& selectedComputers );

private:
	static void populateGroupChooserMenu( QMenu* menu, VeyonMaster& master, QWidget* parent );
	static void updateButton( QToolButton* button, VeyonMaster& master );
	static int presentMemberCount( VeyonMaster& master, int groupIndex );
	static void createGroupFromComputers( VeyonMaster& master, QWidget* parent,
										  const ComputerControlInterfaceList& computers );
	static void showManageDialog( VeyonMaster& master, QWidget* parent );

};
