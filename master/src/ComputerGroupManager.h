/*
 * ComputerGroupManager.h - manually defined groups of computers (ITLabControl)
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
#include <QSet>
#include <QUuid>

#include "ComputerControlInterface.h"

class ComputerMonitoringModel;
class UserConfig;

// Teachers split the class into groups by hand. Choosing a group shows only its computers
// in the monitoring view, so every toolbar feature then applies to that group only.
// Groups are stored in the teacher's personal configuration (UI/ComputerGroups).
class ComputerGroupManager : public QObject
{
	Q_OBJECT
public:
	struct Group
	{
		QString name;
		QSet<QUuid> members;
	};
	using Groups = QList<Group>;

	static constexpr int AllComputers = -1;

	ComputerGroupManager( UserConfig& userConfig, ComputerMonitoringModel* monitoringModel, QObject* parent );

	const Groups& groups() const
	{
		return m_groups;
	}

	void setGroups( const Groups& groups );

	int addGroup( const QString& name, const ComputerControlInterfaceList& computers );
	void addToGroup( int groupIndex, const ComputerControlInterfaceList& computers );
	void removeFromGroup( int groupIndex, const ComputerControlInterfaceList& computers );

	int activeGroup() const
	{
		return m_activeGroup;
	}

	void setActiveGroup( int groupIndex );

	QString uniqueGroupName() const;

	static QSet<QUuid> uids( const ComputerControlInterfaceList& computers );

Q_SIGNALS:
	void groupsChanged();
	void activeGroupChanged();

private:
	void load();
	void save();
	void applyFilter();

	UserConfig& m_userConfig;
	ComputerMonitoringModel* m_monitoringModel;
	Groups m_groups;
	int m_activeGroup{AllComputers};

};
