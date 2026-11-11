/*
 * RoomManagerDialog.cpp - cửa sổ "Quản lý phòng" của Bảng điều khiển giáo viên
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

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QHostInfo>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QProgressBar>
#include <QRegularExpression>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTcpSocket>
#include <QTimer>
#include <QTreeWidget>
#include <QUuid>
#include <QVBoxLayout>

#include "Computer.h"
#include "ItlabRoomsPlugin.h"
#include "NetworkObject.h"
#include "NetworkObjectDirectory.h"
#include "NetworkObjectDirectoryManager.h"
#include "RoomManagerDialog.h"
#include "VeyonConfiguration.h"
#include "VeyonMasterInterface.h"


namespace {

// các khoá JSON của BuiltinDirectory (xem NetworkObject::toJson())
const auto keyType = QStringLiteral("Type");
const auto keyUid = QStringLiteral("Uid");
const auto keyName = QStringLiteral("Name");
const auto keyHostAddress = QStringLiteral("HostAddress");
const auto keyMacAddress = QStringLiteral("MacAddress");
const auto keyParentUid = QStringLiteral("ParentUid");

constexpr int RoleUid = Qt::UserRole;
constexpr int RoleIsRoom = Qt::UserRole + 1;

constexpr int PortScanTimeout = 2000;
constexpr int QueryTimeout = 10000;
constexpr int SaveTimeout = 10000;

bool isLocation( const QJsonObject& object )
{
	return object.value( keyType ).toInt() == static_cast<int>( NetworkObject::Type::Location );
}

bool isHost( const QJsonObject& object )
{
	return object.value( keyType ).toInt() == static_cast<int>( NetworkObject::Type::Host );
}

QString newUid()
{
	return QUuid::createUuid().toString();
}

}



RoomManagerDialog::RoomManagerDialog( VeyonMasterInterface& master, ItlabRoomsPlugin* plugin ) :
	QDialog( master.mainWindow() ),
	m_master( master ),
	m_plugin( plugin )
{
	setWindowTitle( tr( "Rooms" ) );
	setWindowIcon( QIcon( QStringLiteral(":/itlab-rooms/itlab-rooms.png") ) );
	resize( 900, 600 );

	auto tabs = new QTabWidget( this );
	tabs->addTab( createRoomsTab(), tr( "Rooms and computers" ) );
	tabs->addTab( createPatternTab(), tr( "Add by name pattern" ) );
	tabs->addTab( createDiscoveryTab(), tr( "Discover computers" ) );

	m_statusLabel = new QLabel( this );
	m_statusLabel->setWordWrap( true );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Save | QDialogButtonBox::Close, this );
	connect( buttons->button( QDialogButtonBox::Save ), &QPushButton::clicked, this, &RoomManagerDialog::save );
	connect( buttons->button( QDialogButtonBox::Close ), &QPushButton::clicked, this, &RoomManagerDialog::reject );

	auto layout = new QVBoxLayout( this );
	layout->addWidget( tabs );
	layout->addWidget( m_statusLabel );
	layout->addWidget( buttons );

	loadDirectory();
}



RoomManagerDialog::~RoomManagerDialog()
{
	qDeleteAll( m_scanSockets );
}



// ═════════════════════════════════════════════════════════════════════════════
//  Danh sách phòng/máy
// ═════════════════════════════════════════════════════════════════════════════

void RoomManagerDialog::loadDirectory()
{
	VeyonCore::config().reloadFromStore();
	m_objects = VeyonCore::config().value( QStringLiteral("NetworkObjects"), QStringLiteral("BuiltinDirectory"),
										   QJsonArray() ).toJsonArray();
	m_modified = false;

	refreshRoomsTree();
	refreshRoomComboBoxes();
}



QString RoomManagerDialog::findLocationUid( const QString& roomName ) const
{
	const auto normalized = ItlabRoomsPlugin::normalizedRoomName( roomName );
	for( const auto& value : m_objects )
	{
		const auto object = value.toObject();
		if( isLocation( object ) && ItlabRoomsPlugin::normalizedRoomName( object.value( keyName ).toString() ) == normalized )
		{
			return object.value( keyUid ).toString();
		}
	}
	return {};
}



QString RoomManagerDialog::roomNameOf( const QString& locationUid ) const
{
	for( const auto& value : m_objects )
	{
		const auto object = value.toObject();
		if( isLocation( object ) && object.value( keyUid ).toString() == locationUid )
		{
			return object.value( keyName ).toString();
		}
	}
	return {};
}



QString RoomManagerDialog::ensureLocation( const QString& roomName )
{
	auto uid = findLocationUid( roomName );
	if( uid.isEmpty() )
	{
		uid = newUid();
		m_objects.append( QJsonObject{ { keyType, static_cast<int>( NetworkObject::Type::Location ) },
									   { keyUid, uid },
									   { keyName, roomName.simplified() } } );
		setModified();
	}
	return uid;
}



int RoomManagerDialog::findComputerIndex( const QString& name, const QString& hostAddress ) const
{
	for( int i = 0; i < m_objects.size(); ++i )
	{
		const auto object = m_objects[i].toObject();
		if( isHost( object ) &&
			( object.value( keyName ).toString().compare( name, Qt::CaseInsensitive ) == 0 ||
			  ( hostAddress.isEmpty() == false &&
				object.value( keyHostAddress ).toString().compare( hostAddress, Qt::CaseInsensitive ) == 0 ) ) )
		{
			return i;
		}
	}
	return -1;
}



void RoomManagerDialog::addOrMoveComputer( const QString& name, const QString& hostAddress, const QString& macAddress,
										   const QString& locationUid )
{
	// máy đã có (cùng tên hoặc cùng địa chỉ) thì cập nhật và chuyển phòng, không tạo bản trùng
	const auto index = findComputerIndex( name, hostAddress );
	QJsonObject object = index >= 0 ? m_objects[index].toObject()
									: QJsonObject{ { keyType, static_cast<int>( NetworkObject::Type::Host ) },
												   { keyUid, newUid() } };
	object[keyName] = name;
	object[keyHostAddress] = hostAddress.isEmpty() ? name : hostAddress;
	if( macAddress.isEmpty() == false )
	{
		object[keyMacAddress] = macAddress;
	}
	object[keyParentUid] = locationUid;

	if( index >= 0 )
	{
		m_objects[index] = object;
	}
	else
	{
		m_objects.append( object );
	}
	setModified();
}



QStringList RoomManagerDialog::roomNames() const
{
	QStringList names;
	for( const auto& value : m_objects )
	{
		const auto object = value.toObject();
		if( isLocation( object ) )
		{
			names.append( object.value( keyName ).toString() );
		}
	}
	names.sort( Qt::CaseInsensitive );
	return names;
}



void RoomManagerDialog::setModified()
{
	m_modified = true;
	showStatus( tr( "There are unsaved changes. Click \"Save\" to apply them." ) );
}



void RoomManagerDialog::showStatus( const QString& text )
{
	m_statusLabel->setText( text );
}



void RoomManagerDialog::refreshRoomsTree()
{
	m_roomsTree->clear();

	QMap<QString, QTreeWidgetItem*> roomItems;
	for( const auto& value : std::as_const( m_objects ) )
	{
		const auto object = value.toObject();
		if( isLocation( object ) )
		{
			auto item = new QTreeWidgetItem( m_roomsTree, { object.value( keyName ).toString() } );
			item->setData( 0, RoleUid, object.value( keyUid ).toString() );
			item->setData( 0, RoleIsRoom, true );
			item->setIcon( 0, QIcon( QStringLiteral(":/itlab-rooms/itlab-rooms.png") ) );
			roomItems[object.value( keyUid ).toString()] = item;
		}
	}

	for( const auto& value : std::as_const( m_objects ) )
	{
		const auto object = value.toObject();
		if( isHost( object ) )
		{
			auto parent = roomItems.value( object.value( keyParentUid ).toString() );
			const QStringList columns{ object.value( keyName ).toString(),
									   object.value( keyHostAddress ).toString(),
									   object.value( keyMacAddress ).toString() };
			auto item = parent ? new QTreeWidgetItem( parent, columns ) : new QTreeWidgetItem( m_roomsTree, columns );
			item->setData( 0, RoleUid, object.value( keyUid ).toString() );
			item->setData( 0, RoleIsRoom, false );
		}
	}

	m_roomsTree->sortItems( 0, Qt::AscendingOrder );
	m_roomsTree->expandAll();
	for( int i = 0; i < m_roomsTree->columnCount(); ++i )
	{
		m_roomsTree->resizeColumnToContents( i );
	}
}



void RoomManagerDialog::refreshRoomComboBoxes()
{
	const auto names = roomNames();
	for( auto comboBox : { m_patternRoom, m_discoveryRoom } )
	{
		const auto current = comboBox->currentText();
		comboBox->clear();
		comboBox->addItems( names );
		comboBox->setEditText( current );
	}
}



// ═════════════════════════════════════════════════════════════════════════════
//  Thẻ "Phòng và máy"
// ═════════════════════════════════════════════════════════════════════════════

QWidget* RoomManagerDialog::createRoomsTab()
{
	auto tab = new QWidget;

	m_roomsTree = new QTreeWidget( tab );
	m_roomsTree->setHeaderLabels( { tr( "Room / computer" ), tr( "Host address" ), tr( "MAC address" ) } );
	m_roomsTree->setSelectionMode( QAbstractItemView::ExtendedSelection );

	auto addRoomButton = new QPushButton( tr( "Add room" ), tab );
	auto renameRoomButton = new QPushButton( tr( "Rename room" ), tab );
	auto addComputersButton = new QPushButton( tr( "Add computers by name" ), tab );
	auto moveButton = new QPushButton( tr( "Move to room" ), tab );
	auto writeRoomButton = new QPushButton( tr( "Save room on student computers" ), tab );
	auto removeButton = new QPushButton( tr( "Remove" ), tab );

	writeRoomButton->setToolTip( tr( "Writes the room name into the configuration of the selected computers, "
									 "so they report this room when computers are discovered." ) );

	connect( addRoomButton, &QPushButton::clicked, this, &RoomManagerDialog::addRoom );
	connect( renameRoomButton, &QPushButton::clicked, this, &RoomManagerDialog::renameRoom );
	connect( addComputersButton, &QPushButton::clicked, this, &RoomManagerDialog::addComputersByName );
	connect( moveButton, &QPushButton::clicked, this, &RoomManagerDialog::moveSelectedComputers );
	connect( writeRoomButton, &QPushButton::clicked, this, &RoomManagerDialog::writeRoomToSelectedComputers );
	connect( removeButton, &QPushButton::clicked, this, &RoomManagerDialog::removeSelected );

	auto buttonLayout = new QVBoxLayout;
	for( auto button : { addRoomButton, renameRoomButton, addComputersButton, moveButton, writeRoomButton, removeButton } )
	{
		buttonLayout->addWidget( button );
	}
	buttonLayout->addStretch( 1 );

	auto layout = new QHBoxLayout( tab );
	layout->addWidget( m_roomsTree, 1 );
	layout->addLayout( buttonLayout );

	return tab;
}



void RoomManagerDialog::addRoom()
{
	const auto name = QInputDialog::getText( this, tr( "Add room" ), tr( "Room name:" ) ).simplified();
	if( name.isEmpty() )
	{
		return;
	}
	if( findLocationUid( name ).isEmpty() == false )
	{
		QMessageBox::information( this, tr( "Add room" ), tr( "A room with this name already exists." ) );
		return;
	}

	ensureLocation( name );
	refreshRoomsTree();
	refreshRoomComboBoxes();
}



void RoomManagerDialog::renameRoom()
{
	const auto item = m_roomsTree->currentItem();
	if( item == nullptr || item->data( 0, RoleIsRoom ).toBool() == false )
	{
		QMessageBox::information( this, tr( "Rename room" ), tr( "Please select a room first." ) );
		return;
	}

	const auto uid = item->data( 0, RoleUid ).toString();
	const auto name = QInputDialog::getText( this, tr( "Rename room" ), tr( "Room name:" ),
											 QLineEdit::Normal, item->text( 0 ) ).simplified();
	if( name.isEmpty() || name == item->text( 0 ) )
	{
		return;
	}
	const auto existing = findLocationUid( name );
	if( existing.isEmpty() == false && existing != uid )
	{
		QMessageBox::information( this, tr( "Rename room" ), tr( "A room with this name already exists." ) );
		return;
	}

	for( int i = 0; i < m_objects.size(); ++i )
	{
		auto object = m_objects[i].toObject();
		if( object.value( keyUid ).toString() == uid )
		{
			object[keyName] = name;
			m_objects[i] = object;
		}
	}
	setModified();
	refreshRoomsTree();
	refreshRoomComboBoxes();
}



void RoomManagerDialog::removeSelected()
{
	const auto items = m_roomsTree->selectedItems();
	if( items.isEmpty() )
	{
		return;
	}

	if( QMessageBox::question( this, tr( "Remove" ),
							   tr( "Remove the selected rooms and computers? Removing a room also removes "
								   "all computers in it." ) ) != QMessageBox::Yes )
	{
		return;
	}

	QStringList uids;
	for( const auto item : items )
	{
		uids.append( item->data( 0, RoleUid ).toString() );
	}

	QJsonArray remaining;
	for( const auto& value : std::as_const( m_objects ) )
	{
		const auto object = value.toObject();
		if( uids.contains( object.value( keyUid ).toString() ) ||
			uids.contains( object.value( keyParentUid ).toString() ) )
		{
			continue;
		}
		remaining.append( object );
	}
	m_objects = remaining;

	setModified();
	refreshRoomsTree();
	refreshRoomComboBoxes();
}



void RoomManagerDialog::addComputersByName()
{
	auto item = m_roomsTree->currentItem();
	if( item && item->data( 0, RoleIsRoom ).toBool() == false )
	{
		item = item->parent();
	}
	if( item == nullptr )
	{
		QMessageBox::information( this, tr( "Add computers by name" ), tr( "Please select a room first." ) );
		return;
	}

	const auto text = QInputDialog::getMultiLineText( this, tr( "Add computers by name" ),
		tr( "Enter one computer per line, either as name only (e.g. T36-ICT2-20) "
			"or as name and IP address separated by a comma (e.g. T36-ICT2-20, 172.18.217.20):" ) );

	const auto uid = item->data( 0, RoleUid ).toString();
	int count = 0;
	for( const auto& line : text.split( QLatin1Char('\n'), Qt::SkipEmptyParts ) )
	{
		const auto parts = line.split( QLatin1Char(',') );
		const auto name = parts.value( 0 ).trimmed();
		const auto address = parts.value( 1 ).trimmed();
		if( name.isEmpty() == false )
		{
			addOrMoveComputer( name, address, {}, uid );
			++count;
		}
	}

	if( count > 0 )
	{
		refreshRoomsTree();
		showStatus( tr( "%1 computers added to room \"%2\". Click \"Save\" to apply." ).arg( count ).arg( item->text( 0 ) ) );
	}
}



void RoomManagerDialog::moveSelectedComputers()
{
	QStringList uids;
	for( const auto item : m_roomsTree->selectedItems() )
	{
		if( item->data( 0, RoleIsRoom ).toBool() == false )
		{
			uids.append( item->data( 0, RoleUid ).toString() );
		}
	}
	if( uids.isEmpty() )
	{
		QMessageBox::information( this, tr( "Move to room" ), tr( "Please select one or more computers first." ) );
		return;
	}

	bool ok = false;
	const auto room = QInputDialog::getItem( this, tr( "Move to room" ), tr( "Target room:" ), roomNames(), 0, true, &ok );
	if( ok == false || room.simplified().isEmpty() )
	{
		return;
	}

	const auto locationUid = ensureLocation( room );
	for( int i = 0; i < m_objects.size(); ++i )
	{
		auto object = m_objects[i].toObject();
		if( uids.contains( object.value( keyUid ).toString() ) )
		{
			object[keyParentUid] = locationUid;
			m_objects[i] = object;
		}
	}
	setModified();
	refreshRoomsTree();
	refreshRoomComboBoxes();
}



void RoomManagerDialog::writeRoomToSelectedComputers()
{
	int count = 0;
	for( const auto item : m_roomsTree->selectedItems() )
	{
		if( item->data( 0, RoleIsRoom ).toBool() == false && item->parent() )
		{
			sendSetRoom( item->text( 1 ), item->text( 0 ), item->parent()->text( 0 ) );
			++count;
		}
	}

	if( count == 0 )
	{
		QMessageBox::information( this, tr( "Save room on student computers" ),
								  tr( "Please select one or more computers in a room first." ) );
	}
}



// ═════════════════════════════════════════════════════════════════════════════
//  Thẻ "Tạo theo mẫu tên"
// ═════════════════════════════════════════════════════════════════════════════

QWidget* RoomManagerDialog::createPatternTab()
{
	auto tab = new QWidget;

	m_patternRoom = new QComboBox( tab );
	m_patternRoom->setEditable( true );
	m_patternPrefix = new QLineEdit( tab );
	m_patternPrefix->setPlaceholderText( tr( "e.g. T36-ICT2-" ) );
	m_patternFrom = new QSpinBox( tab );
	m_patternFrom->setRange( 0, 9999 );
	m_patternFrom->setValue( 1 );
	m_patternTo = new QSpinBox( tab );
	m_patternTo->setRange( 0, 9999 );
	m_patternTo->setValue( 31 );
	m_patternDigits = new QSpinBox( tab );
	m_patternDigits->setRange( 1, 4 );
	m_patternDigits->setValue( 2 );
	m_patternDigits->setToolTip( tr( "Minimum number of digits, e.g. 2 gives 01, 02, ... and 1 gives 1, 2, ..." ) );
	m_patternSuffix = new QLineEdit( tab );
	m_patternSuffix->setPlaceholderText( tr( "optional" ) );
	m_patternPreview = new QListWidget( tab );

	auto addButton = new QPushButton( tr( "Add to room" ), tab );
	connect( addButton, &QPushButton::clicked, this, &RoomManagerDialog::addPatternComputers );

	for( auto lineEdit : { m_patternPrefix, m_patternSuffix } )
	{
		connect( lineEdit, &QLineEdit::textChanged, this, &RoomManagerDialog::updatePatternPreview );
	}
	for( auto spinBox : { m_patternFrom, m_patternTo, m_patternDigits } )
	{
		connect( spinBox, QOverload<int>::of( &QSpinBox::valueChanged ), this, &RoomManagerDialog::updatePatternPreview );
	}

	auto numbers = new QHBoxLayout;
	numbers->addWidget( m_patternFrom );
	numbers->addWidget( new QLabel( tr( "to" ), tab ) );
	numbers->addWidget( m_patternTo );
	numbers->addStretch( 1 );

	auto form = new QFormLayout;
	form->addRow( tr( "Room:" ), m_patternRoom );
	form->addRow( tr( "Name prefix:" ), m_patternPrefix );
	form->addRow( tr( "Numbers:" ), numbers );
	form->addRow( tr( "Digits:" ), m_patternDigits );
	form->addRow( tr( "Name suffix:" ), m_patternSuffix );

	auto hint = new QLabel( tr( "Computers are added by name. Unwanted entries can be removed in the preview "
								"(select and press Delete)." ), tab );
	hint->setWordWrap( true );

	auto layout = new QVBoxLayout( tab );
	layout->addLayout( form );
	layout->addWidget( hint );
	layout->addWidget( new QLabel( tr( "Preview:" ), tab ) );
	layout->addWidget( m_patternPreview, 1 );
	layout->addWidget( addButton, 0, Qt::AlignRight );

	// xoá dòng xem trước bằng phím Delete
	auto deleteShortcut = new QAction( m_patternPreview );
	deleteShortcut->setShortcut( QKeySequence::Delete );
	deleteShortcut->setShortcutContext( Qt::WidgetShortcut );
	m_patternPreview->addAction( deleteShortcut );
	m_patternPreview->setSelectionMode( QAbstractItemView::ExtendedSelection );
	connect( deleteShortcut, &QAction::triggered, this, [this]() { qDeleteAll( m_patternPreview->selectedItems() ); } );

	return tab;
}



QStringList RoomManagerDialog::patternNames() const
{
	QStringList names;
	const auto prefix = m_patternPrefix->text().trimmed();
	const auto suffix = m_patternSuffix->text().trimmed();
	if( prefix.isEmpty() && suffix.isEmpty() )
	{
		return names;
	}

	const auto from = std::min( m_patternFrom->value(), m_patternTo->value() );
	const auto to = std::max( m_patternFrom->value(), m_patternTo->value() );
	for( int n = from; n <= to && names.size() < 1000; ++n )
	{
		names.append( prefix + QStringLiteral("%1").arg( n, m_patternDigits->value(), 10, QLatin1Char('0') ) + suffix );
	}
	return names;
}



void RoomManagerDialog::updatePatternPreview()
{
	m_patternPreview->clear();
	m_patternPreview->addItems( patternNames() );
}



void RoomManagerDialog::addPatternComputers()
{
	const auto room = m_patternRoom->currentText().simplified();
	if( room.isEmpty() )
	{
		QMessageBox::information( this, tr( "Add to room" ), tr( "Please enter or select a room." ) );
		return;
	}
	if( m_patternPreview->count() == 0 )
	{
		QMessageBox::information( this, tr( "Add to room" ), tr( "Please enter a name prefix and number range." ) );
		return;
	}

	const auto locationUid = ensureLocation( room );
	for( int i = 0; i < m_patternPreview->count(); ++i )
	{
		addOrMoveComputer( m_patternPreview->item( i )->text(), {}, {}, locationUid );
	}

	refreshRoomsTree();
	refreshRoomComboBoxes();
	showStatus( tr( "%1 computers added to room \"%2\". Click \"Save\" to apply." )
				.arg( m_patternPreview->count() ).arg( room ) );
}



// ═════════════════════════════════════════════════════════════════════════════
//  Thẻ "Tự dò máy"
// ═════════════════════════════════════════════════════════════════════════════

QWidget* RoomManagerDialog::createDiscoveryTab()
{
	auto tab = new QWidget;

	m_scanRange = new QLineEdit( defaultScanRange(), tab );
	m_scanRange->setToolTip( tr( "Examples: 172.18.217.1-254, 10.0.0.0/24, 10.0.1.5 (separate multiple entries by commas)" ) );
	m_scanButton = new QPushButton( tr( "Discover" ), tab );
	connect( m_scanButton, &QPushButton::clicked, this, &RoomManagerDialog::startDiscovery );

	m_scanProgress = new QProgressBar( tab );
	m_scanProgress->setVisible( false );

	m_discoveredTree = new QTreeWidget( tab );
	m_discoveredTree->setRootIsDecorated( false );
	m_discoveredTree->setHeaderLabels( { tr( "Computer name" ), tr( "IP address" ), tr( "MAC address" ),
										 tr( "Room (computer)" ), tr( "User" ), tr( "Status" ) } );
	m_discoveredTree->setSortingEnabled( true );

	m_discoveryRoom = new QComboBox( tab );
	m_discoveryRoom->setEditable( true );
	m_writeRoomToComputers = new QCheckBox( tr( "Also save the room on the student computers" ), tab );
	m_writeRoomToComputers->setChecked( true );

	auto addToRoomButton = new QPushButton( tr( "Add checked computers to room" ), tab );
	auto addByDeclaredRoomButton = new QPushButton( tr( "Add checked computers to their own rooms" ), tab );
	addByDeclaredRoomButton->setToolTip( tr( "Uses the room configured on each computer; missing rooms are created." ) );
	connect( addToRoomButton, &QPushButton::clicked, this, [this]() { addDiscoveredComputers( false ); } );
	connect( addByDeclaredRoomButton, &QPushButton::clicked, this, [this]() { addDiscoveredComputers( true ); } );

	auto rangeLayout = new QHBoxLayout;
	rangeLayout->addWidget( new QLabel( tr( "Network range:" ), tab ) );
	rangeLayout->addWidget( m_scanRange, 1 );
	rangeLayout->addWidget( m_scanButton );

	auto addLayout = new QHBoxLayout;
	addLayout->addWidget( new QLabel( tr( "Room:" ), tab ) );
	addLayout->addWidget( m_discoveryRoom, 1 );
	addLayout->addWidget( addToRoomButton );
	addLayout->addWidget( addByDeclaredRoomButton );

	auto hint = new QLabel( tr( "Only computers with ITLabControl and the same authentication key answer. "
								"Other computers (e.g. with Veyon) are shown as not accessible." ), tab );
	hint->setWordWrap( true );

	auto layout = new QVBoxLayout( tab );
	layout->addLayout( rangeLayout );
	layout->addWidget( hint );
	layout->addWidget( m_scanProgress );
	layout->addWidget( m_discoveredTree, 1 );
	layout->addLayout( addLayout );
	layout->addWidget( m_writeRoomToComputers );

	return tab;
}



QString RoomManagerDialog::defaultScanRange()
{
	QStringList ranges;
	const auto interfaces = QNetworkInterface::allInterfaces();
	for( const auto& networkInterface : interfaces )
	{
		if( networkInterface.flags().testFlag( QNetworkInterface::IsUp ) == false ||
			networkInterface.flags().testFlag( QNetworkInterface::IsLoopBack ) ||
			networkInterface.type() == QNetworkInterface::Virtual )
		{
			continue;
		}
		for( const auto& entry : networkInterface.addressEntries() )
		{
			const auto ip = entry.ip();
			if( ip.protocol() == QAbstractSocket::IPv4Protocol && ip.isLinkLocal() == false )
			{
				const auto parts = ip.toString().split( QLatin1Char('.') );
				ranges.append( QStringLiteral("%1.%2.%3.1-254").arg( parts[0], parts[1], parts[2] ) );
			}
		}
	}
	ranges.removeDuplicates();
	return ranges.join( QStringLiteral(", ") );
}



QStringList RoomManagerDialog::parseAddressRanges( const QString& ranges )
{
	QStringList addresses;
	static const QRegularExpression separator( QStringLiteral("[,;\\s]+") );
	for( const auto& range : ranges.split( separator, Qt::SkipEmptyParts ) )
	{
		quint32 first = 0;
		quint32 last = 0;
		if( range.contains( QLatin1Char('/') ) )
		{
			const auto subnet = QHostAddress::parseSubnet( range );
			if( subnet.first.protocol() != QAbstractSocket::IPv4Protocol || subnet.second < 16 )
			{
				continue;
			}
			const auto mask = subnet.second == 0 ? 0u : ( 0xffffffffu << ( 32 - subnet.second ) );
			first = ( subnet.first.toIPv4Address() & mask ) + 1;
			last = ( subnet.first.toIPv4Address() | ~mask ) - 1;
		}
		else if( range.contains( QLatin1Char('-') ) )
		{
			const auto start = QHostAddress( range.section( QLatin1Char('-'), 0, 0 ) );
			auto endText = range.section( QLatin1Char('-'), 1, 1 );
			if( endText.contains( QLatin1Char('.') ) == false )
			{
				endText = range.section( QLatin1Char('-'), 0, 0 ).section( QLatin1Char('.'), 0, 2 ) + QLatin1Char('.') + endText;
			}
			const auto end = QHostAddress( endText );
			if( start.protocol() != QAbstractSocket::IPv4Protocol || end.protocol() != QAbstractSocket::IPv4Protocol )
			{
				continue;
			}
			first = start.toIPv4Address();
			last = end.toIPv4Address();
		}
		else
		{
			const QHostAddress address( range );
			if( address.protocol() != QAbstractSocket::IPv4Protocol )
			{
				continue;
			}
			first = last = address.toIPv4Address();
		}

		for( auto ip = first; ip <= last && addresses.size() < 4096; ++ip )
		{
			addresses.append( QHostAddress( ip ).toString() );
		}
	}
	addresses.removeDuplicates();
	return addresses;
}



void RoomManagerDialog::startDiscovery()
{
	const auto addresses = parseAddressRanges( m_scanRange->text() );
	if( addresses.isEmpty() )
	{
		QMessageBox::information( this, tr( "Discover computers" ), tr( "Please enter a valid network range." ) );
		return;
	}

	// bỏ kết quả lần trước
	qDeleteAll( m_scanSockets );
	m_scanSockets.clear();
	m_openAddresses.clear();
	m_discovered.clear();
	m_discoveredTree->clear();

	m_scanButton->setEnabled( false );
	m_scanProgress->setVisible( true );
	m_scanProgress->setRange( 0, 0 );
	showStatus( tr( "Searching %1 addresses..." ).arg( addresses.size() ) );

	// bước 1: thử kết nối nhanh tới cổng của ITLabControl trên mọi địa chỉ cùng lúc
	const auto port = quint16( VeyonCore::config().veyonServerPort() );
	for( const auto& address : addresses )
	{
		auto socket = new QTcpSocket;
		connect( socket, &QTcpSocket::connected, this, [this, address, socket]() {
			m_openAddresses.append( address );
			socket->abort();
		} );
		socket->connectToHost( address, port );
		m_scanSockets.append( socket );
	}

	QTimer::singleShot( PortScanTimeout, this, &RoomManagerDialog::finishPortScan );
}



void RoomManagerDialog::finishPortScan()
{
	qDeleteAll( m_scanSockets );
	m_scanSockets.clear();

	if( m_openAddresses.isEmpty() )
	{
		finishDiscovery();
		return;
	}

	// bước 2: kết nối có xác thực tới các máy đang mở cổng và hỏi thông tin
	m_scanProgress->setRange( 0, m_openAddresses.size() );
	m_scanProgress->setValue( 0 );
	showStatus( tr( "Querying %1 computers..." ).arg( m_openAddresses.size() ) );

	for( const auto& address : std::as_const( m_openAddresses ) )
	{
		queryComputer( address );
	}

	QTimer::singleShot( QueryTimeout, this, &RoomManagerDialog::finishDiscovery );
}



void RoomManagerDialog::queryComputer( const QString& ipAddress )
{
	DiscoveredComputer computer;
	computer.ipAddress = ipAddress;
	computer.item = new QTreeWidgetItem( m_discoveredTree, { QString(), ipAddress, QString(), QString(), QString(),
															 tr( "Connecting..." ) } );
	computer.controlInterface = ComputerControlInterface::Pointer::create(
		Computer( NetworkObject::Uid::createUuid(), ipAddress, ipAddress ) );

	const auto controlInterface = computer.controlInterface;
	connect( controlInterface.data(), &ComputerControlInterface::stateChanged, this, [this, ipAddress]() {
		auto it = m_discovered.find( ipAddress );
		if( it == m_discovered.end() || it->answered )
		{
			return;
		}
		switch( it->controlInterface->state() )
		{
		case ComputerControlInterface::State::Connected:
			it->controlInterface->sendFeatureMessage(
				FeatureMessage{ m_plugin->featureUid(), ItlabRoomsPlugin::FeatureCommand::QueryInfo } );
			it->item->setText( 5, tr( "Querying..." ) );
			break;
		case ComputerControlInterface::State::AuthenticationFailed:
			it->item->setText( 5, tr( "Not accessible (other authentication key, e.g. Veyon)" ) );
			break;
		case ComputerControlInterface::State::AccessControlFailed:
			it->item->setText( 5, tr( "Access denied" ) );
			break;
		default:
			break;
		}
	} );

	m_discovered.insert( ipAddress, computer );
	controlInterface->start( {}, ComputerControlInterface::UpdateMode::FeatureControlOnly );
}



void RoomManagerDialog::finishDiscovery()
{
	if( m_scanButton->isEnabled() )
	{
		return; // đã kết thúc
	}

	int found = 0;
	for( auto it = m_discovered.begin(); it != m_discovered.end(); ++it )
	{
		if( it->answered )
		{
			++found;
		}
		else
		{
			if( it->item->text( 5 ) == tr( "Connecting..." ) || it->item->text( 5 ) == tr( "Querying..." ) )
			{
				it->item->setText( 5, tr( "No answer" ) );
			}
			it->item->setFlags( it->item->flags() & ~Qt::ItemIsUserCheckable );
			it->controlInterface.clear();
		}
	}

	m_scanButton->setEnabled( true );
	m_scanProgress->setVisible( false );
	for( int i = 0; i < m_discoveredTree->columnCount(); ++i )
	{
		m_discoveredTree->resizeColumnToContents( i );
	}
	showStatus( tr( "%1 computers with ITLabControl found." ).arg( found ) );
}



void RoomManagerDialog::addDiscoveredComputers( bool byDeclaredRoom )
{
	const auto targetRoom = m_discoveryRoom->currentText().simplified();
	if( byDeclaredRoom == false && targetRoom.isEmpty() )
	{
		QMessageBox::information( this, tr( "Discover computers" ), tr( "Please enter or select a room." ) );
		return;
	}

	int added = 0;
	int withoutRoom = 0;
	for( auto it = m_discovered.begin(); it != m_discovered.end(); ++it )
	{
		if( it->answered == false || it->item->checkState( 0 ) != Qt::Checked )
		{
			continue;
		}

		const auto room = byDeclaredRoom ? it->room : targetRoom;
		if( room.isEmpty() )
		{
			++withoutRoom;
			continue;
		}

		// lưu bằng tên máy nếu tên tra ra đúng địa chỉ IP (không lo IP DHCP đổi), ngược lại lưu bằng IP
		const auto hostAddress = it->nameResolvesToAddress ? it->hostName : it->ipAddress;
		addOrMoveComputer( it->hostName, hostAddress, it->macAddress, ensureLocation( room ) );
		++added;

		if( m_writeRoomToComputers->isChecked() &&
			ItlabRoomsPlugin::normalizedRoomName( it->room ) != ItlabRoomsPlugin::normalizedRoomName( room ) )
		{
			sendSetRoom( it->ipAddress, it->hostName, room );
		}
	}

	refreshRoomsTree();
	refreshRoomComboBoxes();

	auto status = tr( "%1 computers added. Click \"Save\" to apply." ).arg( added );
	if( withoutRoom > 0 )
	{
		status += QLatin1Char(' ') + tr( "%1 computers were skipped because no room is configured on them." ).arg( withoutRoom );
	}
	showStatus( status );
}



// ═════════════════════════════════════════════════════════════════════════════
//  Trả lời từ máy HS/GV
// ═════════════════════════════════════════════════════════════════════════════

void RoomManagerDialog::handleReply( ComputerControlInterface::Pointer computerControlInterface,
									 const FeatureMessage& message )
{
	using Command = ItlabRoomsPlugin::FeatureCommand;
	using Argument = ItlabRoomsPlugin::Argument;

	switch( message.command<Command>() )
	{
	case Command::InfoReply:
		for( auto it = m_discovered.begin(); it != m_discovered.end(); ++it )
		{
			if( it->controlInterface.data() != computerControlInterface.data() || it->answered )
			{
				continue;
			}

			it->answered = true;
			it->hostName = message.argument( Argument::HostName ).toString();
			it->room = message.argument( Argument::Room ).toString();
			it->user = message.argument( Argument::UserLogin ).toString();
			const auto interfaces = message.argument( Argument::Interfaces ).toStringList();
			for( const auto& entry : interfaces )
			{
				if( entry.section( QLatin1Char('='), 0, 0 ) == it->ipAddress || it->macAddress.isEmpty() )
				{
					it->macAddress = entry.section( QLatin1Char('='), 1 );
				}
			}

			QString status = tr( "Ready" );
			const auto index = findComputerIndex( it->hostName, it->ipAddress );
			if( index >= 0 )
			{
				status = tr( "Already in room \"%1\"" ).arg(
					roomNameOf( m_objects[index].toObject().value( keyParentUid ).toString() ) );
			}

			it->item->setText( 0, it->hostName );
			it->item->setText( 2, it->macAddress );
			it->item->setText( 3, it->room );
			it->item->setText( 4, it->user );
			it->item->setText( 5, status );
			it->item->setFlags( it->item->flags() | Qt::ItemIsUserCheckable );
			it->item->setCheckState( 0, index >= 0 ? Qt::Unchecked : Qt::Checked );
			m_scanProgress->setValue( m_scanProgress->value() + 1 );

			// kiểm tra tên máy có tra ra đúng IP không (cùng subnet: mDNS/LLMNR)
			const auto ipAddress = it->ipAddress;
			QHostInfo::lookupHost( it->hostName, this, [this, ipAddress]( const QHostInfo& info ) {
				auto computer = m_discovered.find( ipAddress );
				if( computer != m_discovered.end() )
				{
					for( const auto& address : info.addresses() )
					{
						if( address.toString() == ipAddress )
						{
							computer->nameResolvesToAddress = true;
						}
					}
				}
			} );
			break;
		}
		break;

	case Command::SetRoomReply:
		for( auto& pending : m_pendingSetRoom )
		{
			if( pending.controlInterface.data() == computerControlInterface.data() )
			{
				pending.controlInterface.clear();
			}
		}
		showStatus( tr( "Room \"%1\" saved on computer %2." )
					.arg( message.argument( Argument::Room ).toString(),
						  computerControlInterface ? computerControlInterface->computer().displayName() : QString() ) );
		break;

	case Command::SetDirectoryReply:
		if( m_saving == false )
		{
			break;
		}
		m_saving = false;
		if( message.argument( Argument::Success ).toBool() )
		{
			m_modified = false;
			if( auto directory = VeyonCore::networkObjectDirectoryManager().configuredDirectory() )
			{
				directory->update();
			}
			showStatus( tr( "Rooms and computers saved." ) );
		}
		else
		{
			QMessageBox::critical( this, tr( "Save" ), tr( "The rooms and computers could not be saved." ) );
		}
		break;

	default:
		break;
	}
}



// ═════════════════════════════════════════════════════════════════════════════
//  Ghi tên phòng vào cấu hình máy HS từ xa
// ═════════════════════════════════════════════════════════════════════════════

void RoomManagerDialog::sendSetRoom( const QString& hostAddress, const QString& displayName, const QString& room )
{
	// (FeatureMessage không sao chép được bằng "auto x = ...", nên tạo ngay trong lời gọi gửi)
	const auto sendTo = [this, &room]( const ComputerControlInterface::Pointer& controlInterface ) {
		controlInterface->sendFeatureMessage(
			FeatureMessage{ m_plugin->featureUid(), ItlabRoomsPlugin::FeatureCommand::SetRoom }
				.addArgument( ItlabRoomsPlugin::Argument::Room, room ) );
	};

	// 1. máy vừa được dò thấy: dùng luôn kết nối đó
	const auto discovered = m_discovered.find( hostAddress );
	if( discovered != m_discovered.end() && discovered->controlInterface &&
		discovered->controlInterface->state() == ComputerControlInterface::State::Connected )
	{
		sendTo( discovered->controlInterface );
		m_pendingSetRoom.append( { discovered->controlInterface, displayName, room, true } );
		return;
	}

	// 2. máy đang hiện trên Console: dùng kết nối của Console
	for( const auto& controlInterface : m_master.allComputerControlInterfaces() )
	{
		const auto& computer = controlInterface->computer();
		if( ( computer.hostName().compare( hostAddress, Qt::CaseInsensitive ) == 0 ||
			  computer.hostAddress().toString() == hostAddress ) &&
			controlInterface->state() == ComputerControlInterface::State::Connected )
		{
			sendTo( controlInterface );
			return;
		}
	}

	// 3. tạo kết nối tạm, gửi lệnh khi kết nối xong
	auto controlInterface = ComputerControlInterface::Pointer::create(
		Computer( NetworkObject::Uid::createUuid(), displayName, hostAddress ) );
	m_pendingSetRoom.append( { controlInterface, displayName, room, false } );

	connect( controlInterface.data(), &ComputerControlInterface::stateChanged, this,
			 [this, rawPointer = controlInterface.data()]() {
		for( auto& pending : m_pendingSetRoom )
		{
			if( pending.controlInterface.data() != rawPointer || pending.sent )
			{
				continue;
			}
			const auto state = pending.controlInterface->state();
			if( state == ComputerControlInterface::State::Connected )
			{
				pending.controlInterface->sendFeatureMessage(
					FeatureMessage{ m_plugin->featureUid(), ItlabRoomsPlugin::FeatureCommand::SetRoom }
						.addArgument( ItlabRoomsPlugin::Argument::Room, pending.room ) );
				pending.sent = true;
			}
			else if( state == ComputerControlInterface::State::HostOffline ||
					 state == ComputerControlInterface::State::HostNameResolutionFailed ||
					 state == ComputerControlInterface::State::ServerNotRunning ||
					 state == ComputerControlInterface::State::AuthenticationFailed ||
					 state == ComputerControlInterface::State::AccessControlFailed )
			{
				showStatus( tr( "Could not save the room on computer %1 (not reachable)." ).arg( pending.displayName ) );
				pending.sent = true;
			}
		}
	} );

	controlInterface->start( {}, ComputerControlInterface::UpdateMode::FeatureControlOnly );
	showStatus( tr( "Saving room \"%1\" on computer %2..." ).arg( room, displayName ) );
}



// ═════════════════════════════════════════════════════════════════════════════
//  Lưu
// ═════════════════════════════════════════════════════════════════════════════

void RoomManagerDialog::save()
{
	// danh sách được ghi bởi dịch vụ ITLabControl trên chính máy này (quyền SYSTEM),
	// nên giáo viên không cần quyền admin
	auto& localSession = m_master.localSessionControlInterface();
	if( localSession.state() != ComputerControlInterface::State::Connected )
	{
		QMessageBox::critical( this, tr( "Save" ),
							   tr( "The ITLabControl service on this computer is not reachable. Please check that "
								   "the service is running and this computer accepts connections from itself." ) );
		return;
	}

	m_saving = true;
	localSession.sendFeatureMessage(
		FeatureMessage{ m_plugin->featureUid(), ItlabRoomsPlugin::FeatureCommand::SetDirectory }
			.addArgument( ItlabRoomsPlugin::Argument::Directory,
						  QString::fromUtf8( QJsonDocument( m_objects ).toJson( QJsonDocument::Compact ) ) ) );
	showStatus( tr( "Saving..." ) );

	QTimer::singleShot( SaveTimeout, this, [this]() {
		if( m_saving )
		{
			m_saving = false;
			QMessageBox::critical( this, tr( "Save" ), tr( "The ITLabControl service on this computer did not answer." ) );
		}
	} );
}



void RoomManagerDialog::reject()
{
	if( m_modified )
	{
		const auto answer = QMessageBox::question( this, tr( "Rooms" ),
												   tr( "There are unsaved changes. Save them now?" ),
												   QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel );
		if( answer == QMessageBox::Cancel )
		{
			return;
		}
		if( answer == QMessageBox::Save )
		{
			save();
			return;
		}
	}

	QDialog::reject();
}
