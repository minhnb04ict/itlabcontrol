/*
 * ComputerGroupManager.cpp - manually defined groups of computers (ITLabControl)
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

#include <QJsonArray>
#include <QJsonObject>

#include "ComputerGroupManager.h"
#include "ComputerMonitoringModel.h"
#include "UserConfig.h"


ComputerGroupManager::ComputerGroupManager( UserConfig& userConfig, ComputerMonitoringModel* monitoringModel,
											QObject* parent ) :
	QObject( parent ),
	m_userConfig( userConfig ),
	m_monitoringModel( monitoringModel )
{
	load();
}



void ComputerGroupManager::setGroups( const Groups& groups )
{
	m_groups = groups;
	save();

	if( m_activeGroup >= m_groups.size() )
	{
		m_activeGroup = AllComputers;
		Q_EMIT activeGroupChanged();
	}

	applyFilter();
	Q_EMIT groupsChanged();
}



int ComputerGroupManager::addGroup( const QString& name, const ComputerControlInterfaceList& computers )
{
	auto groups = m_groups;
	groups.append( { name, uids( computers ) } );
	setGroups( groups );
	return int( m_groups.size() ) - 1;
}



void ComputerGroupManager::addToGroup( int groupIndex, const ComputerControlInterfaceList& computers )
{
	if( groupIndex >= 0 && groupIndex < m_groups.size() )
	{
		auto groups = m_groups;
		groups[groupIndex].members.unite( uids( computers ) );
		setGroups( groups );
	}
}



void ComputerGroupManager::removeFromGroup( int groupIndex, const ComputerControlInterfaceList& computers )
{
	if( groupIndex >= 0 && groupIndex < m_groups.size() )
	{
		auto groups = m_groups;
		groups[groupIndex].members.subtract( uids( computers ) );
		setGroups( groups );
	}
}



void ComputerGroupManager::setActiveGroup( int groupIndex )
{
	if( groupIndex < 0 || groupIndex >= m_groups.size() )
	{
		groupIndex = AllComputers;
	}

	if( groupIndex != m_activeGroup )
	{
		m_activeGroup = groupIndex;
		applyFilter();
		Q_EMIT activeGroupChanged();
	}
}



QString ComputerGroupManager::uniqueGroupName() const
{
	for( int i = 1; ; ++i )
	{
		const auto name = tr( "Group %1" ).arg( i );
		if( std::none_of( m_groups.cbegin(), m_groups.cend(),
						  [&name]( const Group& group ) { return group.name == name; } ) )
		{
			return name;
		}
	}
}



QSet<QUuid> ComputerGroupManager::uids( const ComputerControlInterfaceList& computers )
{
	QSet<QUuid> result;
	for( const auto& computer : computers )
	{
		if( computer )
		{
			result.insert( computer->computer().networkObjectUid() );
		}
	}
	return result;
}



void ComputerGroupManager::load()
{
	m_groups.clear();

	const auto groups = m_userConfig.computerGroups();
	for( const auto& groupValue : groups )
	{
		const auto groupObject = groupValue.toObject();
		Group group{ groupObject.value( QLatin1String("Name") ).toString(), {} };
		const auto members = groupObject.value( QLatin1String("Members") ).toArray();
		for( const auto& member : members )
		{
			const QUuid uid( member.toString() );
			if( uid.isNull() == false )
			{
				group.members.insert( uid );
			}
		}
		if( group.name.isEmpty() == false )
		{
			m_groups.append( group );
		}
	}
}



void ComputerGroupManager::save()
{
	QJsonArray groups;
	for( const auto& group : std::as_const(m_groups) )
	{
		QJsonArray members;
		for( const auto& uid : group.members )
		{
			members.append( uid.toString() );
		}
		groups.append( QJsonObject{ { QLatin1String("Name"), group.name },
									{ QLatin1String("Members"), members } } );
	}

	m_userConfig.setComputerGroups( groups );
	m_userConfig.flushStore();
}



void ComputerGroupManager::applyFilter()
{
	if( m_activeGroup >= 0 && m_activeGroup < m_groups.size() )
	{
		m_monitoringModel->setGroupFilter( true, m_groups[m_activeGroup].members );
	}
	else if( m_monitoringModel->groupFilterEnabled() )
	{
		m_monitoringModel->setGroupFilter( false, {} );
	}
}
