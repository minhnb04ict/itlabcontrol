/*
 * RoomConfigurationPage.cpp - trang "Phòng máy" trong Configurator
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

#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include "ItlabRoomsPlugin.h"
#include "RoomConfigurationPage.h"
#include "VeyonConfiguration.h"


RoomConfigurationPage::RoomConfigurationPage( QWidget* parent ) :
	ConfigurationPage( parent ),
	m_roomEdit( new QLineEdit( this ) )
{
	setWindowTitle( tr( "Room" ) );
	setWindowIcon( QIcon( QStringLiteral(":/itlab-rooms/itlab-rooms.png") ) );

	auto description = new QLabel( tr( "Enter the room this computer belongs to. When the teacher discovers computers "
										"in the Teacher Console, computers are assigned to rooms automatically by this "
										"name. Upper/lower case, Vietnamese diacritics and extra spaces are ignored." ),
								   this );
	description->setWordWrap( true );

	m_roomEdit->setPlaceholderText( tr( "e.g. Room 201" ) );

	auto form = new QFormLayout;
	form->addRow( tr( "Room of this computer:" ), m_roomEdit );

	auto layout = new QVBoxLayout( this );
	layout->addWidget( description );
	layout->addLayout( form );
	layout->addStretch( 1 );
}



void RoomConfigurationPage::resetWidgets()
{
	m_roomEdit->setText( ItlabRoomsPlugin::configuredRoom() );
}



void RoomConfigurationPage::connectWidgetsToProperties()
{
	// thay đổi được ghi vào cấu hình đang mở; Configurator lưu khi bấm "Áp dụng"
	connect( m_roomEdit, &QLineEdit::textChanged, this, []( const QString& room ) {
		VeyonCore::config().setValue( QStringLiteral("Room"), room.simplified(), QStringLiteral("ITLab") );
	} );
}



void RoomConfigurationPage::applyConfiguration()
{
}
