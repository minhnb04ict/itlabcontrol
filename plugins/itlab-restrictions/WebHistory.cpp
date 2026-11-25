/*
 * WebHistory.cpp - keep the web browsing history of student computers for 30 days (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

#include "Filesystem.h"
#include "VeyonConfiguration.h"
#include "WebHistory.h"


namespace {

QMutex s_mutex;

// Chromium stores microseconds since 1601-01-01, Firefox microseconds since 1970-01-01
constexpr qint64 ChromiumEpochOffsetMs = 11644473600000LL;

QString storageDirectory()
{
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/webhistory") );
}

QString stateFile()
{
	return storageDirectory() + QStringLiteral("/state.json");
}

struct HistoryDatabase
{
	QString path;
	QString browser;
	QString user;
	bool firefox{false};
};

QList<HistoryDatabase> findDatabases()
{
	QList<HistoryDatabase> databases;

	static const QList<QPair<QString, QString>> chromiumBrowsers{
		{ QStringLiteral("Chrome"), QStringLiteral("AppData/Local/Google/Chrome/User Data") },
		{ QStringLiteral("Edge"), QStringLiteral("AppData/Local/Microsoft/Edge/User Data") },
		{ QStringLiteral("Coc Coc"), QStringLiteral("AppData/Local/CocCoc/Browser/User Data") },
		{ QStringLiteral("Brave"), QStringLiteral("AppData/Local/BraveSoftware/Brave-Browser/User Data") },
	};
	static const QStringList skippedUsers{ QStringLiteral("public"), QStringLiteral("default"),
										   QStringLiteral("default user"), QStringLiteral("all users") };

	const QDir usersDir( qEnvironmentVariable( "SystemDrive", QStringLiteral("C:") ) + QStringLiteral("/Users") );
	const auto users = usersDir.entryList( QDir::Dirs | QDir::NoDotAndDotDot );
	for( const auto& user : users )
	{
		if( skippedUsers.contains( user.toLower() ) )
		{
			continue;
		}
		const auto userPath = usersDir.filePath( user );

		for( const auto& browser : chromiumBrowsers )
		{
			const QDir userData( userPath + QLatin1Char('/') + browser.second );
			const auto profiles = userData.entryList( { QStringLiteral("Default"), QStringLiteral("Profile *") }, QDir::Dirs );
			for( const auto& profile : profiles )
			{
				const auto path = userData.filePath( profile + QStringLiteral("/History") );
				if( QFileInfo::exists( path ) )
				{
					databases.append( { path, browser.first, user, false } );
				}
			}
		}

		const QDir firefoxProfiles( userPath + QStringLiteral("/AppData/Roaming/Mozilla/Firefox/Profiles") );
		const auto profiles = firefoxProfiles.entryList( QDir::Dirs | QDir::NoDotAndDotDot );
		for( const auto& profile : profiles )
		{
			const auto path = firefoxProfiles.filePath( profile + QStringLiteral("/places.sqlite") );
			if( QFileInfo::exists( path ) )
			{
				databases.append( { path, QStringLiteral("Firefox"), user, true } );
			}
		}
	}

	return databases;
}

QString cleanField( QString text )
{
	text.replace( QLatin1Char('\t'), QLatin1Char(' ') ).replace( QLatin1Char('\n'), QLatin1Char(' ') ).replace( QLatin1Char('\r'), QLatin1Char(' ') );
	return text.left( 2000 );
}

void appendEntries( const QMap<QString, QStringList>& linesPerDay )
{
	for( auto it = linesPerDay.begin(); it != linesPerDay.end(); ++it )
	{
		QFile file( storageDirectory() + QLatin1Char('/') + it.key() + QStringLiteral(".tsv") );
		if( file.open( QFile::Append ) )
		{
			file.write( it.value().join( QLatin1Char('\n') ).toUtf8() + '\n' );
		}
	}
}

void pruneOldFiles()
{
	const auto oldestDay = QDate::currentDate().addDays( -WebHistory::retentionDays() );
	const QDir dir( storageDirectory() );
	const auto files = dir.entryList( { QStringLiteral("*.tsv") }, QDir::Files );
	for( const auto& file : files )
	{
		const auto day = QDate::fromString( file.left( 10 ), Qt::ISODate );
		if( day.isValid() && day < oldestDay )
		{
			QFile::remove( dir.filePath( file ) );
		}
	}
}

}



int WebHistory::retentionDays()
{
	const auto days = VeyonCore::config().value( QStringLiteral("WebHistoryDays"), QStringLiteral("ITLab"), 30 ).toInt();
	return days > 0 ? days : 30;
}



void WebHistory::collect()
{
	QMutexLocker locker( &s_mutex );

	QDir().mkpath( storageDirectory() );

	QFile stateIn( stateFile() );
	auto state = stateIn.open( QFile::ReadOnly ) ? QJsonDocument::fromJson( stateIn.readAll() ).object() : QJsonObject{};
	stateIn.close();

	const auto oldestMs = QDateTime::currentDateTime().addDays( -retentionDays() ).toMSecsSinceEpoch();
	QMap<QString, QStringList> linesPerDay;

	for( const auto& database : findDatabases() )
	{
		// skip unchanged databases (Firefox writes to the -wal file first)
		const auto walPath = database.path + QStringLiteral("-wal");
		const auto modified = std::max( QFileInfo( database.path ).lastModified().toMSecsSinceEpoch(),
										QFileInfo::exists( walPath ) ? QFileInfo( walPath ).lastModified().toMSecsSinceEpoch() : 0 );
		auto entry = state.value( database.path ).toObject();
		if( entry.value( QStringLiteral("modified") ).toVariant().toLongLong() == modified )
		{
			continue;
		}

		// the browser keeps its database locked - work on a copy
		QTemporaryDir tempDir;
		if( tempDir.isValid() == false )
		{
			continue;
		}
		const auto copy = tempDir.filePath( QStringLiteral("history.sqlite") );
		if( QFile::copy( database.path, copy ) == false )
		{
			continue;
		}
		if( QFileInfo::exists( walPath ) )
		{
			QFile::copy( walPath, copy + QStringLiteral("-wal") );
		}

		// last stored visit in the format of the browser; on first use import the retention period
		auto last = entry.value( QStringLiteral("last") ).toVariant().toLongLong();
		if( last <= 0 )
		{
			last = database.firefox ? oldestMs * 1000 : ( oldestMs + ChromiumEpochOffsetMs ) * 1000;
		}
		auto newest = last;

		const auto connectionName = QUuid::createUuid().toString();
		{
			auto db = QSqlDatabase::addDatabase( QStringLiteral("QSQLITE"), connectionName );
			db.setDatabaseName( copy );
			if( db.open() )
			{
				QSqlQuery query( db );
				query.prepare( database.firefox ?
					QStringLiteral("SELECT v.visit_date, p.url, p.title FROM moz_historyvisits v "
								   "JOIN moz_places p ON v.place_id = p.id WHERE v.visit_date > ? ORDER BY v.visit_date") :
					QStringLiteral("SELECT v.visit_time, u.url, u.title FROM visits v "
								   "JOIN urls u ON v.url = u.id WHERE v.visit_time > ? ORDER BY v.visit_time") );
				query.addBindValue( last );
				if( query.exec() )
				{
					while( query.next() )
					{
						const auto raw = query.value( 0 ).toLongLong();
						newest = std::max( newest, raw );
						const auto url = query.value( 1 ).toString();
						if( url.startsWith( QLatin1String("http://") ) == false && url.startsWith( QLatin1String("https://") ) == false )
						{
							continue;
						}
						const auto ms = database.firefox ? raw / 1000 : raw / 1000 - ChromiumEpochOffsetMs;
						const auto day = QDateTime::fromMSecsSinceEpoch( ms ).date().toString( Qt::ISODate );
						linesPerDay[day].append( QStringList{ QString::number( ms ), cleanField( database.user ), database.browser,
															  cleanField( url ), cleanField( query.value( 2 ).toString() ) }
												 .join( QLatin1Char('\t') ) );
					}
				}
				db.close();
			}
		}
		QSqlDatabase::removeDatabase( connectionName );

		entry[QStringLiteral("last")] = newest;
		entry[QStringLiteral("modified")] = modified;
		state[database.path] = entry;
	}

	appendEntries( linesPerDay );
	pruneOldFiles();

	QSaveFile stateOut( stateFile() );
	if( stateOut.open( QFile::WriteOnly ) )
	{
		stateOut.write( QJsonDocument( state ).toJson() );
		stateOut.commit();
	}
}



QVariantList WebHistory::query( qint64 fromMs, qint64 toMs, int maxEntries )
{
	QMutexLocker locker( &s_mutex );

	QVariantList entries;
	const auto firstDay = QDateTime::fromMSecsSinceEpoch( fromMs ).date();
	const auto lastDay = QDateTime::fromMSecsSinceEpoch( toMs ).date();

	for( auto day = firstDay; day <= lastDay && day.isValid(); day = day.addDays( 1 ) )
	{
		QFile file( storageDirectory() + QLatin1Char('/') + day.toString( Qt::ISODate ) + QStringLiteral(".tsv") );
		if( file.open( QFile::ReadOnly ) == false )
		{
			continue;
		}
		while( file.atEnd() == false )
		{
			const auto fields = QString::fromUtf8( file.readLine() ).trimmed().split( QLatin1Char('\t') );
			if( fields.size() < 5 )
			{
				continue;
			}
			const auto ms = fields[0].toLongLong();
			if( ms < fromMs || ms > toMs )
			{
				continue;
			}
			entries.append( QVariant( QVariantList{ ms, fields[1], fields[2], fields[3], fields[4] } ) );
		}
	}

	std::sort( entries.begin(), entries.end(), []( const QVariant& a, const QVariant& b ) {
		return a.toList().value( 0 ).toLongLong() < b.toList().value( 0 ).toLongLong();
	} );

	// keep the most recent entries
	if( entries.size() > maxEntries )
	{
		entries = entries.mid( entries.size() - maxEntries );
	}

	return entries;
}
