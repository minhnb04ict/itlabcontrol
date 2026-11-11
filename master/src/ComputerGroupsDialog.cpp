/*
 * ComputerGroupsDialog.cpp - dialog for editing computer groups (ITLabControl)
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

#include <QCollator>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "ComputerGroupsDialog.h"


static constexpr int UidRole = Qt::UserRole + 1;


ComputerGroupsDialog::ComputerGroupsDialog( ComputerGroupManager& manager,
											const ComputerControlInterfaceList& computers,
											int initialGroup, QWidget* parent ) :
	QDialog( parent ),
	m_manager( manager ),
	m_computers( computers ),
	m_groups( manager.groups() ),
	m_groupList( new QListWidget( this ) ),
	m_computerList( new QListWidget( this ) ),
	m_membersLabel( new QLabel( this ) ),
	m_renameButton( new QPushButton( tr( "Rename" ), this ) ),
	m_removeButton( new QPushButton( tr( "Remove" ), this ) ),
	m_checkAllButton( new QPushButton( tr( "Select all" ), this ) ),
	m_uncheckAllButton( new QPushButton( tr( "Select none" ), this ) )
{
	setWindowTitle( tr( "Manage groups" ) );
	resize( 720, 520 );

	QCollator collator;
	collator.setNumericMode( true );
	std::sort( m_computers.begin(), m_computers.end(),
			   [&collator]( const ComputerControlInterface::Pointer& a, const ComputerControlInterface::Pointer& b ) {
				   return collator.compare( a->computer().displayName(), b->computer().displayName() ) < 0;
			   } );

	// left side: groups
	auto addButton = new QPushButton( tr( "Add group" ), this );
	auto groupButtons = new QHBoxLayout;
	groupButtons->addWidget( addButton );
	groupButtons->addWidget( m_renameButton );
	groupButtons->addWidget( m_removeButton );

	auto groupLayout = new QVBoxLayout;
	groupLayout->addWidget( new QLabel( tr( "Groups" ), this ) );
	groupLayout->addWidget( m_groupList );
	groupLayout->addLayout( groupButtons );

	// right side: members of the current group
	auto checkButtons = new QHBoxLayout;
	checkButtons->addWidget( m_checkAllButton );
	checkButtons->addWidget( m_uncheckAllButton );
	checkButtons->addStretch();

	auto memberLayout = new QVBoxLayout;
	memberLayout->addWidget( m_membersLabel );
	memberLayout->addWidget( m_computerList );
	memberLayout->addLayout( checkButtons );

	auto mainLayout = new QHBoxLayout;
	mainLayout->addLayout( groupLayout, 2 );
	mainLayout->addLayout( memberLayout, 3 );

	auto hint = new QLabel( tr( "Tick the computers which belong to the selected group. "
								"A computer may belong to several groups. "
								"Choose a group in the main window to apply all functions to this group only." ), this );
	hint->setWordWrap( true );

	auto buttonBox = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this );

	auto layout = new QVBoxLayout( this );
	layout->addLayout( mainLayout );
	layout->addWidget( hint );
	layout->addWidget( buttonBox );

	for( const auto& computer : std::as_const(m_computers) )
	{
		auto item = new QListWidgetItem( m_computerList );
		item->setData( UidRole, computer->computer().networkObjectUid() );
		item->setFlags( Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable );
		item->setCheckState( Qt::Unchecked );
	}

	connect( addButton, &QPushButton::clicked, this, &ComputerGroupsDialog::addGroup );
	connect( m_renameButton, &QPushButton::clicked, this, &ComputerGroupsDialog::renameGroup );
	connect( m_removeButton, &QPushButton::clicked, this, &ComputerGroupsDialog::removeGroup );
	connect( m_groupList, &QListWidget::itemDoubleClicked, this, &ComputerGroupsDialog::renameGroup );
	connect( m_groupList, &QListWidget::currentRowChanged, this, [this]( int row ) {
		if( m_updating == false )
		{
			storeMembers();
			showGroup( row );
		}
	} );
	connect( m_computerList, &QListWidget::itemChanged, this, [this]() {
		if( m_updating == false )
		{
			storeMembers();
			updateMemberCount();
		}
	} );
	connect( m_checkAllButton, &QPushButton::clicked, this, [this]() { selectAllComputers( true ); } );
	connect( m_uncheckAllButton, &QPushButton::clicked, this, [this]() { selectAllComputers( false ); } );
	connect( buttonBox, &QDialogButtonBox::accepted, this, &ComputerGroupsDialog::accept );
	connect( buttonBox, &QDialogButtonBox::rejected, this, &ComputerGroupsDialog::reject );

	updateGroupList( m_groups.isEmpty() ? -1 : qBound( 0, initialGroup, int(m_groups.size()) - 1 ) );
}



void ComputerGroupsDialog::accept()
{
	storeMembers();
	m_manager.setGroups( m_groups );
	QDialog::accept();
}



// rebuilds the group list and shows the given group; the caller has already stored the check states
void ComputerGroupsDialog::updateGroupList( int currentGroup )
{
	currentGroup = qMin( currentGroup, int(m_groups.size()) - 1 );

	m_updating = true;
	m_groupList->clear();
	for( const auto& group : std::as_const(m_groups) )
	{
		m_groupList->addItem( group.name );
	}
	m_groupList->setCurrentRow( currentGroup );
	m_updating = false;

	showGroup( currentGroup );
}



void ComputerGroupsDialog::showGroup( int groupIndex )
{
	m_updating = true;

	m_currentGroup = groupIndex;
	const auto valid = groupIndex >= 0 && groupIndex < m_groups.size();

	for( int i = 0; i < m_computerList->count(); ++i )
	{
		auto item = m_computerList->item( i );
		const auto uid = item->data( UidRole ).toUuid();
		const auto& computer = m_computers[i];

		auto text = computer->computer().displayName();
		const auto others = otherGroupNames( uid, groupIndex );
		if( others.isEmpty() == false )
		{
			text += QStringLiteral("   (%1)").arg( others );
		}
		item->setText( text );
		item->setCheckState( valid && m_groups[groupIndex].members.contains( uid ) ? Qt::Checked : Qt::Unchecked );
	}

	m_computerList->setEnabled( valid );
	m_renameButton->setEnabled( valid );
	m_removeButton->setEnabled( valid );
	m_checkAllButton->setEnabled( valid );
	m_uncheckAllButton->setEnabled( valid );

	m_updating = false;

	updateMemberCount();
}



void ComputerGroupsDialog::storeMembers()
{
	if( m_currentGroup < 0 || m_currentGroup >= m_groups.size() )
	{
		return;
	}

	// keep members which are not listed here (e.g. computers of another room)
	auto& members = m_groups[m_currentGroup].members;
	for( int i = 0; i < m_computerList->count(); ++i )
	{
		const auto item = m_computerList->item( i );
		const auto uid = item->data( UidRole ).toUuid();
		if( item->checkState() == Qt::Checked )
		{
			members.insert( uid );
		}
		else
		{
			members.remove( uid );
		}
	}
}



void ComputerGroupsDialog::updateMemberCount()
{
	if( m_currentGroup < 0 || m_currentGroup >= m_groups.size() )
	{
		m_membersLabel->setText( m_groups.isEmpty() ? tr( "Click \"Add group\" to create the first group." )
													: tr( "Select a group on the left." ) );
		return;
	}

	int count = 0;
	for( int i = 0; i < m_computerList->count(); ++i )
	{
		if( m_computerList->item( i )->checkState() == Qt::Checked )
		{
			++count;
		}
	}

	m_membersLabel->setText( tr( "Computers in group \"%1\": %2" ).arg( m_groups[m_currentGroup].name ).arg( count ) );
}



void ComputerGroupsDialog::addGroup()
{
	storeMembers();

	// propose a name which is not used yet
	QString proposal;
	for( int i = int(m_groups.size()) + 1; ; ++i )
	{
		proposal = tr( "Group %1" ).arg( i );
		if( std::none_of( m_groups.cbegin(), m_groups.cend(),
						  [&proposal]( const ComputerGroupManager::Group& g ) { return g.name == proposal; } ) )
		{
			break;
		}
	}

	bool ok = false;
	const auto name = QInputDialog::getText( this, tr( "Add group" ), tr( "Group name:" ),
											 QLineEdit::Normal, proposal, &ok ).trimmed();
	if( ok == false || name.isEmpty() )
	{
		return;
	}

	m_groups.append( { name, {} } );
	updateGroupList( int(m_groups.size()) - 1 );
}



void ComputerGroupsDialog::renameGroup()
{
	if( m_currentGroup < 0 || m_currentGroup >= m_groups.size() )
	{
		return;
	}

	bool ok = false;
	const auto name = QInputDialog::getText( this, tr( "Rename group" ), tr( "Group name:" ),
											 QLineEdit::Normal, m_groups[m_currentGroup].name, &ok ).trimmed();
	if( ok && name.isEmpty() == false )
	{
		storeMembers();
		m_groups[m_currentGroup].name = name;
		updateGroupList( m_currentGroup );
	}
}



void ComputerGroupsDialog::removeGroup()
{
	if( m_currentGroup < 0 || m_currentGroup >= m_groups.size() )
	{
		return;
	}

	if( QMessageBox::question( this, tr( "Remove group" ),
							   tr( "Do you really want to remove the group \"%1\"? "
								   "The computers themselves are not affected." ).arg( m_groups[m_currentGroup].name ) )
		!= QMessageBox::Yes )
	{
		return;
	}

	const auto row = m_currentGroup;
	m_groups.removeAt( row );
	m_currentGroup = -1; // do not store the check states into the following group
	updateGroupList( row );
}



void ComputerGroupsDialog::selectAllComputers( bool checked )
{
	m_updating = true;
	for( int i = 0; i < m_computerList->count(); ++i )
	{
		m_computerList->item( i )->setCheckState( checked ? Qt::Checked : Qt::Unchecked );
	}
	m_updating = false;
	storeMembers();
	updateMemberCount();
}



QString ComputerGroupsDialog::otherGroupNames( const QUuid& uid, int exceptGroup ) const
{
	QStringList names;
	for( int i = 0; i < m_groups.size(); ++i )
	{
		if( i != exceptGroup && m_groups[i].members.contains( uid ) )
		{
			names.append( m_groups[i].name );
		}
	}
	return names.join( QStringLiteral(", ") );
}
