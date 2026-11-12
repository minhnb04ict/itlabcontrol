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

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVBoxLayout>

#include "Filesystem.h"
#include "ItlabRoomsPlugin.h"
#include "RoomConfigurationPage.h"
#include "VeyonConfiguration.h"


RoomConfigurationPage::RoomConfigurationPage( QWidget* parent ) :
	ConfigurationPage( parent ),
	m_roomEdit( new QLineEdit( this ) ),
	m_keyStatusLabel( new QLabel( this ) ),
	m_lockScreenPreview( new QLabel( this ) ),
	m_lockScreenTextEdit( new QLineEdit( this ) )
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

	// ITLabControl: access control per room by means of one authentication key pair per room
	auto keyGroup = new QGroupBox( tr( "Control permission per room" ), this );
	auto keyDescription = new QLabel( tr( "Each room uses its own authentication key. A student computer can only be "
										   "controlled by teacher computers which own the key of its room, so the teacher "
										   "of another room can not control it. Computers of the IT staff may additionally "
										   "own an administrator key which is installed on all student computers.\n\n"
										   "1. On the teacher computer: enter the room above, then click \"Create key for this room\" "
										   "and save the public key file to a USB drive.\n"
										   "2. On each student computer of the room: click \"Allow only these keys\" and choose "
										   "the key file(s) from the USB drive." ), keyGroup );
	keyDescription->setWordWrap( true );
	m_keyStatusLabel->setWordWrap( true );
	m_keyStatusLabel->setTextInteractionFlags( Qt::TextSelectableByMouse );

	auto createKeyButton = new QPushButton( tr( "Create key for this room (teacher computer)..." ), keyGroup );
	auto restrictButton = new QPushButton( tr( "Allow only these keys (student computer)..." ), keyGroup );
	connect( createKeyButton, &QPushButton::clicked, this, &RoomConfigurationPage::createRoomKey );
	connect( restrictButton, &QPushButton::clicked, this, &RoomConfigurationPage::restrictToKeys );

	auto buttonLayout = new QHBoxLayout;
	buttonLayout->addWidget( createKeyButton );
	buttonLayout->addWidget( restrictButton );
	buttonLayout->addStretch( 1 );

	auto keyLayout = new QVBoxLayout( keyGroup );
	keyLayout->addWidget( keyDescription );
	keyLayout->addWidget( m_keyStatusLabel );
	keyLayout->addLayout( buttonLayout );

	// ITLabControl: school logo and message on the locked screen of this computer
	auto lockGroup = new QGroupBox( tr( "Lock screen" ), this );
	auto lockDescription = new QLabel( tr( "Image (e.g. the school logo) and text shown on this computer while it is locked "
											"by the teacher. For many computers put the file \"man-hinh-khoa.png\" next to the "
											"student installation script." ), lockGroup );
	lockDescription->setWordWrap( true );
	m_lockScreenPreview->setFixedSize( 200, 120 );
	m_lockScreenPreview->setAlignment( Qt::AlignCenter );
	m_lockScreenPreview->setStyleSheet( QStringLiteral("QLabel { background-color: #404040; }") );
	m_lockScreenTextEdit->setPlaceholderText( tr( "This computer is locked by the teacher" ) );

	auto chooseImageButton = new QPushButton( tr( "Choose image..." ), lockGroup );
	auto defaultImageButton = new QPushButton( tr( "Use default image" ), lockGroup );
	connect( chooseImageButton, &QPushButton::clicked, this, &RoomConfigurationPage::chooseLockScreenImage );
	connect( defaultImageButton, &QPushButton::clicked, this, &RoomConfigurationPage::resetLockScreenImage );

	auto imageButtons = new QVBoxLayout;
	imageButtons->addWidget( chooseImageButton );
	imageButtons->addWidget( defaultImageButton );
	imageButtons->addStretch( 1 );

	auto imageLayout = new QHBoxLayout;
	imageLayout->addWidget( m_lockScreenPreview );
	imageLayout->addLayout( imageButtons );
	imageLayout->addStretch( 1 );

	auto lockForm = new QFormLayout;
	lockForm->addRow( tr( "Text:" ), m_lockScreenTextEdit );

	auto lockLayout = new QVBoxLayout( lockGroup );
	lockLayout->addWidget( lockDescription );
	lockLayout->addLayout( imageLayout );
	lockLayout->addLayout( lockForm );

	auto layout = new QVBoxLayout( this );
	layout->addWidget( description );
	layout->addLayout( form );
	layout->addWidget( keyGroup );
	layout->addWidget( lockGroup );
	layout->addStretch( 1 );

	updateKeyStatus();
	updateLockScreenPreview();
}



QString RoomConfigurationPage::lockScreenImagePath()
{
	// same path as used by the screen lock plugin
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/lock-screen.png") );
}



void RoomConfigurationPage::updateLockScreenPreview()
{
	QPixmap image( lockScreenImagePath() );
	if( image.isNull() )
	{
		image = QPixmap( QStringLiteral(":/screenlock/locked-screen-background.png") );
	}
	if( image.isNull() )
	{
		m_lockScreenPreview->setText( tr( "Default image" ) );
		return;
	}
	m_lockScreenPreview->setPixmap( image.scaled( m_lockScreenPreview->size() - QSize( 8, 8 ),
												  Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
}



void RoomConfigurationPage::chooseLockScreenImage()
{
	const auto file = QFileDialog::getOpenFileName( this, tr( "Choose the image for the lock screen" ),
													QStandardPaths::writableLocation( QStandardPaths::PicturesLocation ),
													tr( "Images (*.png *.jpg *.jpeg *.bmp)" ) );
	if( file.isEmpty() )
	{
		return;
	}

	const QImage image( file );
	const auto targetPath = lockScreenImagePath();
	if( image.isNull() || QDir().mkpath( QFileInfo( targetPath ).absolutePath() ) == false ||
		image.save( targetPath, "PNG" ) == false )
	{
		QMessageBox::critical( this, tr( "Lock screen" ), tr( "The image could not be saved to %1." ).arg( targetPath ) );
		return;
	}

	updateLockScreenPreview();
}



void RoomConfigurationPage::resetLockScreenImage()
{
	QFile::remove( lockScreenImagePath() );
	updateLockScreenPreview();
}



void RoomConfigurationPage::resetWidgets()
{
	m_roomEdit->setText( ItlabRoomsPlugin::configuredRoom() );
	m_lockScreenTextEdit->setText( VeyonCore::config().value( QStringLiteral("LockScreenText"), QStringLiteral("ITLab"),
															  tr( "This computer is locked by the teacher" ) ).toString() );
	updateKeyStatus();
	updateLockScreenPreview();
}



void RoomConfigurationPage::connectWidgetsToProperties()
{
	// thay đổi được ghi vào cấu hình đang mở; Configurator lưu khi bấm "Áp dụng"
	connect( m_roomEdit, &QLineEdit::textChanged, this, []( const QString& room ) {
		VeyonCore::config().setValue( QStringLiteral("Room"), room.simplified(), QStringLiteral("ITLab") );
	} );
	connect( m_lockScreenTextEdit, &QLineEdit::textChanged, this, []( const QString& text ) {
		VeyonCore::config().setValue( QStringLiteral("LockScreenText"), text.simplified(), QStringLiteral("ITLab") );
	} );
}



void RoomConfigurationPage::applyConfiguration()
{
}



QString RoomConfigurationPage::keyNameForRoom( const QString& room )
{
	static const QRegularExpression invalidCharacters( QStringLiteral("[^a-z0-9]+") );
	auto name = ItlabRoomsPlugin::normalizedRoomName( room );
	name.replace( invalidCharacters, QStringLiteral("-") );
	while( name.startsWith( QLatin1Char('-') ) )
	{
		name.remove( 0, 1 );
	}
	while( name.endsWith( QLatin1Char('-') ) )
	{
		name.chop( 1 );
	}
	return name;
}



void RoomConfigurationPage::updateKeyStatus()
{
	const auto privateKeys = keyNames( true );
	const auto publicKeys = keyNames( false );

	const auto none = tr( "(none)" );
	m_keyStatusLabel->setText( tr( "Private keys on this computer (used by the Teacher Console): %1\n"
								   "Keys allowed to control this computer: %2" )
							   .arg( privateKeys.isEmpty() ? none : privateKeys.join( QStringLiteral(", ") ),
									 publicKeys.isEmpty() ? none : publicKeys.join( QStringLiteral(", ") ) ) );
}



void RoomConfigurationPage::createRoomKey()
{
	const auto room = m_roomEdit->text().simplified();
	const auto keyName = keyNameForRoom( room );
	if( keyName.isEmpty() )
	{
		QMessageBox::information( this, tr( "Room key" ), tr( "Please enter the room of this computer first." ) );
		m_roomEdit->setFocus();
		return;
	}

	QString output;
	if( keyNames( true ).contains( keyName ) == false )
	{
		if( runCli( { QStringLiteral("authkeys"), QStringLiteral("create"), keyName }, &output ) == false )
		{
			QMessageBox::critical( this, tr( "Room key" ), tr( "The key could not be created:\n%1" ).arg( output ) );
			return;
		}
	}

	// teachers log on with normal accounts, so they must be able to read the private key
	runCli( { QStringLiteral("authkeys"), QStringLiteral("setaccessgroup"),
			  keyName + QStringLiteral("/private"), QStringLiteral("Users") } );

	const auto directory = QFileDialog::getExistingDirectory( this, tr( "Choose where to save the public key (e.g. a USB drive)" ),
															  QStandardPaths::writableLocation( QStandardPaths::DesktopLocation ) );
	if( directory.isEmpty() == false )
	{
		const auto keyFile = QDir::toNativeSeparators( directory + QStringLiteral("/%1_public_key.pem").arg( keyName ) );
		QFile::remove( keyFile );
		if( runCli( { QStringLiteral("authkeys"), QStringLiteral("export"), keyName + QStringLiteral("/public"), keyFile },
					&output ) == false )
		{
			QMessageBox::critical( this, tr( "Room key" ), tr( "The public key could not be saved:\n%1" ).arg( output ) );
		}
		else
		{
			QMessageBox::information( this, tr( "Room key" ),
									  tr( "The key \"%1\" for room \"%2\" is ready.\n\nThe public key was saved as\n%3\n\n"
										  "Copy this file to the student computers of this room and import it there with "
										  "\"Allow only these keys\" (or put it next to the student installation script)." )
									  .arg( keyName, room, keyFile ) );
		}
	}

	// the Teacher Console uses the first readable private key, so other private keys must go
	auto otherPrivateKeys = keyNames( true );
	otherPrivateKeys.removeAll( keyName );
	if( otherPrivateKeys.isEmpty() == false &&
		QMessageBox::question( this, tr( "Room key" ),
							   tr( "This computer still owns the private keys %1. The Teacher Console might use one of "
								   "them instead of the key of this room and could then control other rooms.\n\n"
								   "Remove these private keys from this computer?" )
							   .arg( otherPrivateKeys.join( QStringLiteral(", ") ) ) ) == QMessageBox::Yes )
	{
		for( const auto& otherKey : std::as_const(otherPrivateKeys) )
		{
			runCli( { QStringLiteral("authkeys"), QStringLiteral("delete"), otherKey + QStringLiteral("/private") } );
		}
	}

	updateKeyStatus();
}



void RoomConfigurationPage::restrictToKeys()
{
	const auto files = QFileDialog::getOpenFileNames( this, tr( "Choose the public key files of the room (and of the IT staff)" ),
													  QStandardPaths::writableLocation( QStandardPaths::DesktopLocation ),
													  tr( "Public key files (*_public_key.pem)" ) );
	if( files.isEmpty() )
	{
		return;
	}

	static const QRegularExpression keyFileRX( QStringLiteral("^(.+)_public_key\\.pem$"), QRegularExpression::CaseInsensitiveOption );
	QStringList newKeys;
	for( const auto& file : files )
	{
		const auto match = keyFileRX.match( QFileInfo( file ).fileName() );
		if( match.hasMatch() == false || VeyonCore::isAuthenticationKeyNameValid( match.captured( 1 ) ) == false )
		{
			QMessageBox::critical( this, tr( "Room key" ), tr( "The file %1 is not a public key file of ITLabControl." ).arg( file ) );
			return;
		}
		newKeys.append( match.captured( 1 ) );
	}

	// keep the public keys of private keys owned by this computer (e.g. on a teacher computer)
	const auto ownKeys = keyNames( true );
	QStringList removedKeys;
	for( const auto& key : keyNames( false ) )
	{
		if( newKeys.contains( key ) == false && ownKeys.contains( key ) == false )
		{
			removedKeys.append( key );
		}
	}

	if( QMessageBox::question( this, tr( "Room key" ),
							   tr( "Only teacher computers owning one of the keys %1 will be able to control this computer.%2\n\nContinue?" )
							   .arg( newKeys.join( QStringLiteral(", ") ),
									 removedKeys.isEmpty() ? QString{} :
															 tr( "\nThe keys %1 will be removed from this computer." )
															 .arg( removedKeys.join( QStringLiteral(", ") ) ) ) ) != QMessageBox::Yes )
	{
		return;
	}

	QString output;
	for( const auto& key : std::as_const(removedKeys) )
	{
		runCli( { QStringLiteral("authkeys"), QStringLiteral("delete"), key + QStringLiteral("/public") } );
	}

	QStringList errors;
	for( int i = 0; i < files.size(); ++i )
	{
		if( keyNames( false ).contains( newKeys[i] ) )
		{
			runCli( { QStringLiteral("authkeys"), QStringLiteral("delete"), newKeys[i] + QStringLiteral("/public") } );
		}
		if( runCli( { QStringLiteral("authkeys"), QStringLiteral("import"), newKeys[i] + QStringLiteral("/public"),
					  QDir::toNativeSeparators( files[i] ) }, &output ) == false )
		{
			errors.append( output );
		}
	}

	// key file authentication must be active for the keys to be used
	VeyonCore::config().setAuthenticationMethod( VeyonCore::AuthenticationMethod::KeyFileAuthentication );
	runCli( { QStringLiteral("config"), QStringLiteral("set"), QStringLiteral("Authentication/Method"), QStringLiteral("1") } );

	updateKeyStatus();

	if( errors.isEmpty() )
	{
		QMessageBox::information( this, tr( "Room key" ), tr( "Done. Only the teacher computers of these keys can control this computer now." ) );
	}
	else
	{
		QMessageBox::critical( this, tr( "Room key" ), tr( "Some keys could not be imported:\n%1" ).arg( errors.join( QLatin1Char('\n') ) ) );
	}
}



QStringList RoomConfigurationPage::keyNames( bool privateKeys )
{
	const auto baseDir = VeyonCore::filesystem().expandPath( privateKeys ? VeyonCore::config().privateKeyBaseDir()
																		 : VeyonCore::config().publicKeyBaseDir() );
	const auto dirs = QDir( baseDir ).entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );

	QStringList names;
	for( const auto& name : dirs )
	{
		const auto path = privateKeys ? VeyonCore::filesystem().privateKeyPath( name )
									  : VeyonCore::filesystem().publicKeyPath( name );
		if( QFileInfo( path ).isFile() )
		{
			names.append( name );
		}
	}
	return names;
}



bool RoomConfigurationPage::runCli( const QStringList& arguments, QString* output )
{
	QProcess process;
	process.setProcessChannelMode( QProcess::MergedChannels );
	// veyon-wcli is the command line tool without console window
	process.start( QCoreApplication::applicationDirPath() + QStringLiteral("/veyon-wcli.exe"), arguments );

	const auto finished = process.waitForFinished( 30000 );
	if( output )
	{
		*output = QString::fromUtf8( process.readAll() ).trimmed(); // veyon-wcli writes UTF-8
	}

	return finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}
