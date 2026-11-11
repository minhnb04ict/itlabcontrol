/*
 * ComputerGroupMenu.cpp - menus for working with computer groups (ITLabControl)
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

#include <QActionGroup>
#include <QInputDialog>
#include <QMenu>
#include <QToolButton>

#include "ComputerControlListModel.h"
#include "ComputerGroupManager.h"
#include "ComputerGroupMenu.h"
#include "ComputerGroupsDialog.h"
#include "VeyonMaster.h"


QToolButton* ComputerGroupMenu::createStatusBarButton( VeyonMaster& master, QWidget* parent )
{
	auto button = new QToolButton( parent );
	button->setObjectName( QStringLiteral("computerGroupButton") );
	button->setAutoRaise( true );
	button->setPopupMode( QToolButton::InstantPopup );
	button->setToolButtonStyle( Qt::ToolButtonTextOnly );
	button->setToolTip( tr( "Choose a group to show only its computers. "
							"All functions of the tool bar then apply to this group only." ) );

	auto menu = new QMenu( button );
	QObject::connect( menu, &QMenu::aboutToShow, button, [menu, &master, parent]() {
		populateGroupChooserMenu( menu, master, parent );
	} );
	button->setMenu( menu );

	auto& groups = master.computerGroups();
	const auto update = [button, &master]() { updateButton( button, master ); };
	QObject::connect( &groups, &ComputerGroupManager::groupsChanged, button, update );
	QObject::connect( &groups, &ComputerGroupManager::activeGroupChanged, button, update );
	QObject::connect( &master.computerControlListModel(), &QAbstractItemModel::modelReset, button, update );
	QObject::connect( &master.computerControlListModel(), &QAbstractItemModel::rowsInserted, button, update );
	QObject::connect( &master.computerControlListModel(), &QAbstractItemModel::rowsRemoved, button, update );

	updateButton( button, master );

	return button;
}



void ComputerGroupMenu::addContextMenu( QMenu* menu, VeyonMaster& master, QWidget* parent,
										const ComputerControlInterfaceList& selectedComputers )
{
	if( selectedComputers.isEmpty() )
	{
		return;
	}

	auto& groups = master.computerGroups();

	auto groupMenu = menu->addMenu( QIcon( QStringLiteral(":/core/user-group-new.png") ), tr( "Groups" ) );

	auto addMenu = groupMenu->addMenu( tr( "Add to group" ) );
	for( int i = 0; i < groups.groups().size(); ++i )
	{
		addMenu->addAction( groups.groups()[i].name, parent, [&groups, i, selectedComputers]() {
			groups.addToGroup( i, selectedComputers );
		} );
	}
	if( groups.groups().isEmpty() == false )
	{
		addMenu->addSeparator();
	}
	addMenu->addAction( tr( "New group..." ), parent, [&master, parent, selectedComputers]() {
		createGroupFromComputers( master, parent, selectedComputers );
	} );

	const auto activeGroup = groups.activeGroup();
	if( activeGroup >= 0 && activeGroup < groups.groups().size() )
	{
		groupMenu->addAction( tr( "Remove from group \"%1\"" ).arg( groups.groups()[activeGroup].name ),
							  parent, [&groups, activeGroup, selectedComputers]() {
								  groups.removeFromGroup( activeGroup, selectedComputers );
							  } );
	}

	groupMenu->addSeparator();
	groupMenu->addAction( tr( "Manage groups..." ), parent, [&master, parent]() { showManageDialog( master, parent ); } );
}



void ComputerGroupMenu::populateGroupChooserMenu( QMenu* menu, VeyonMaster& master, QWidget* parent )
{
	menu->clear();

	auto& groups = master.computerGroups();
	auto actionGroup = new QActionGroup( menu );

	const auto addChoice = [&]( const QString& text, int groupIndex ) {
		auto action = menu->addAction( text, parent, [&groups, groupIndex]() { groups.setActiveGroup( groupIndex ); } );
		action->setCheckable( true );
		action->setChecked( groups.activeGroup() == groupIndex );
		actionGroup->addAction( action );
	};

	addChoice( tr( "All computers (%1)" ).arg( master.allComputerControlInterfaces().size() ),
			   ComputerGroupManager::AllComputers );

	for( int i = 0; i < groups.groups().size(); ++i )
	{
		addChoice( tr( "%1 (%2 computers)" ).arg( groups.groups()[i].name ).arg( presentMemberCount( master, i ) ), i );
	}

	menu->addSeparator();

	const auto selected = master.selectedComputerControlInterfaces();
	auto createAction = menu->addAction( tr( "New group from selected computers..." ), parent, [&master, parent, selected]() {
		createGroupFromComputers( master, parent, selected );
	} );
	createAction->setEnabled( selected.isEmpty() == false );
	if( selected.isEmpty() )
	{
		createAction->setText( tr( "New group from selected computers (select computers first)" ) );
	}

	menu->addAction( tr( "Manage groups..." ), parent, [&master, parent]() { showManageDialog( master, parent ); } );
}



void ComputerGroupMenu::updateButton( QToolButton* button, VeyonMaster& master )
{
	const auto& groups = master.computerGroups();
	const auto activeGroup = groups.activeGroup();

	if( activeGroup >= 0 && activeGroup < groups.groups().size() )
	{
		button->setText( tr( "Group: %1 (%2 computers)" )
						 .arg( groups.groups()[activeGroup].name )
						 .arg( presentMemberCount( master, activeGroup ) ) );
		// make clear that only part of the class is shown and controlled
		button->setStyleSheet( QStringLiteral("QToolButton { background-color: #ffd54f; color: #000000; "
											  "font-weight: bold; border-radius: 3px; padding: 2px 6px; }") );
	}
	else
	{
		button->setText( tr( "Group: all computers" ) );
		button->setStyleSheet( {} );
	}
}



int ComputerGroupMenu::presentMemberCount( VeyonMaster& master, int groupIndex )
{
	const auto members = master.computerGroups().groups().value( groupIndex ).members;

	int count = 0;
	for( const auto& computer : master.allComputerControlInterfaces() )
	{
		if( members.contains( computer->computer().networkObjectUid() ) )
		{
			++count;
		}
	}
	return count;
}



void ComputerGroupMenu::createGroupFromComputers( VeyonMaster& master, QWidget* parent,
												  const ComputerControlInterfaceList& computers )
{
	auto& groups = master.computerGroups();

	bool ok = false;
	const auto name = QInputDialog::getText( parent, tr( "New group" ),
											 tr( "Name of the new group with %1 computers:" ).arg( computers.size() ),
											 QLineEdit::Normal, groups.uniqueGroupName(), &ok ).trimmed();
	if( ok && name.isEmpty() == false )
	{
		groups.addGroup( name, computers );
	}
}



void ComputerGroupMenu::showManageDialog( VeyonMaster& master, QWidget* parent )
{
	auto& groups = master.computerGroups();
	ComputerGroupsDialog dialog( groups, master.allComputerControlInterfaces(),
								 qMax( 0, groups.activeGroup() ), parent );
	dialog.exec();
}
