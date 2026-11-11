/*
 * ComputerGroupsDialog.h - dialog for editing computer groups (ITLabControl)
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

#include <QDialog>

#include "ComputerGroupManager.h"

class QLabel;
class QListWidget;
class QPushButton;

class ComputerGroupsDialog : public QDialog
{
	Q_OBJECT
public:
	ComputerGroupsDialog( ComputerGroupManager& manager, const ComputerControlInterfaceList& computers,
						  int initialGroup, QWidget* parent );

	void accept() override;

private:
	void updateGroupList( int currentGroup );
	void showGroup( int groupIndex );
	void storeMembers();
	void updateMemberCount();

	void addGroup();
	void renameGroup();
	void removeGroup();
	void selectAllComputers( bool checked );

	QString otherGroupNames( const QUuid& uid, int exceptGroup ) const;

	ComputerGroupManager& m_manager;
	ComputerControlInterfaceList m_computers;
	ComputerGroupManager::Groups m_groups;
	int m_currentGroup{-1};
	bool m_updating{false};

	QListWidget* m_groupList;
	QListWidget* m_computerList;
	QLabel* m_membersLabel;
	QPushButton* m_renameButton;
	QPushButton* m_removeButton;
	QPushButton* m_checkAllButton;
	QPushButton* m_uncheckAllButton;

};
