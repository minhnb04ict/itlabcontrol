/*
 * KeyTools.cpp - authentication key helpers for pairing teacher and student computers (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <windows.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include "AuthenticationCredentials.h"
#include "Filesystem.h"
#include "ItlabRoomsPlugin.h"
#include "KeyTools.h"
#include "VeyonConfiguration.h"


QStringList KeyTools::keyNames( bool privateKeys )
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



bool KeyTools::hasTeacherKeys()
{
	const auto ownKeys = keyNames( true );
	for( const auto& key : keyNames( false ) )
	{
		if( ownKeys.contains( key ) == false )
		{
			return true;
		}
	}
	return false;
}



bool KeyTools::teacherPublicKey( QString* keyName, QString* publicKeyPem )
{
	const auto name = VeyonCore::authenticationCredentials().authenticationKeyName();
	if( name.isEmpty() )
	{
		return false;
	}

	QFile file( VeyonCore::filesystem().publicKeyPath( name ) );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return false;
	}

	*keyName = name;
	*publicKeyPem = QString::fromUtf8( file.readAll() );
	return publicKeyPem->contains( QLatin1String("PUBLIC KEY") );
}



bool KeyTools::applyTeacherKey( const QString& keyName, const QString& publicKeyPem, const QString& room, QString* error )
{
	if( VeyonCore::isAuthenticationKeyNameValid( keyName ) == false ||
		publicKeyPem.contains( QLatin1String("-----BEGIN PUBLIC KEY-----") ) == false ||
		publicKeyPem.size() > 16 * 1024 )
	{
		*error = QStringLiteral("invalid key");
		return false;
	}

	QTemporaryDir tempDir;
	if( tempDir.isValid() == false )
	{
		*error = QStringLiteral("no temporary folder");
		return false;
	}

	const auto keyFile = tempDir.filePath( keyName + QStringLiteral("_public_key.pem") );
	QFile file( keyFile );
	if( file.open( QFile::WriteOnly ) == false )
	{
		*error = QStringLiteral("could not write temporary key file");
		return false;
	}
	file.write( publicKeyPem.toUtf8() );
	file.close();

	// replace a key with the same name (e.g. the teacher created a new key)
	if( keyNames( false ).contains( keyName ) )
	{
		runCli( { QStringLiteral("authkeys"), QStringLiteral("delete"), keyName + QStringLiteral("/public") } );
	}

	QString output;
	if( runCli( { QStringLiteral("authkeys"), QStringLiteral("import"), keyName + QStringLiteral("/public"),
				  QDir::toNativeSeparators( keyFile ) }, &output ) == false )
	{
		*error = output;
		return false;
	}

	VeyonCore::config().reloadFromStore();
	VeyonCore::config().setAuthenticationMethod( VeyonCore::AuthenticationMethod::KeyFileAuthentication );
	if( room.simplified().isEmpty() == false )
	{
		VeyonCore::config().setValue( QStringLiteral("Room"), room.simplified(), QStringLiteral("ITLab") );
	}
	VeyonCore::config().flushStore();

	return true;
}



bool KeyTools::runCli( const QStringList& arguments, QString* output )
{
	QProcess process;
	process.setProcessChannelMode( QProcess::MergedChannels );
	process.setCreateProcessArgumentsModifier( []( QProcess::CreateProcessArguments* args ) {
		args->flags |= CREATE_NO_WINDOW;
	} );
	process.start( QCoreApplication::applicationDirPath() + QStringLiteral("/veyon-wcli.exe"), arguments );

	const auto finished = process.waitForFinished( 30000 );
	if( output )
	{
		*output = QString::fromUtf8( process.readAll() ).trimmed();
	}
	return finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}
