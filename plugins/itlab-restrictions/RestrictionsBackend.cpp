/*
 * RestrictionsBackend.cpp - applies and removes restrictions on a student computer (ITLabControl)
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
 */

#include <winsock2.h>
#include <windows.h>
#include <userenv.h>

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>

#include "Filesystem.h"
#include "RestrictionsBackend.h"
#include "VeyonCore.h"


namespace {

// browsers based on Chromium which read the same policies from their own registry key
const QStringList& chromiumPolicyKeys()
{
	static const QStringList keys{
		QStringLiteral("SOFTWARE\\Policies\\Google\\Chrome"),
		QStringLiteral("SOFTWARE\\Policies\\Microsoft\\Edge"),
		QStringLiteral("SOFTWARE\\Policies\\CocCoc\\Browser"),
		QStringLiteral("SOFTWARE\\Policies\\BraveSoftware\\Brave"),
	};
	return keys;
}

const QString firefoxWebsiteFilterKey = QStringLiteral("SOFTWARE\\Policies\\Mozilla\\Firefox\\WebsiteFilter");

const QStringList firewallRuleNames{
	QStringLiteral("ITLabControl Web Block TCP"),
	QStringLiteral("ITLabControl Web Block UDP"),
};

QString stateFilePath()
{
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/restrictions.json") );
}

QString registryPath( const QString& keyPath )
{
	return QStringLiteral("HKEY_LOCAL_MACHINE\\") + keyPath;
}

QStringList sortedValueNames( const QSettings& key )
{
	auto names = key.childKeys();
	QCollator collator;
	collator.setNumericMode( true );
	std::sort( names.begin(), names.end(), collator );
	return names;
}

// canonical entries (see RestrictionsBackend::normalizedWebsites): "[=]host[/path]"
struct WebsiteEntry
{
	bool exact{false};
	QString host;
	QString path;   // "" or "/..."
};

WebsiteEntry parseEntry( const QString& entry )
{
	WebsiteEntry result;
	auto text = entry;
	if( text.startsWith( QLatin1Char('=') ) )
	{
		result.exact = true;
		text = text.mid( 1 );
	}
	const auto slash = text.indexOf( QLatin1Char('/') );
	result.host = slash < 0 ? text : text.left( slash );
	result.path = slash < 0 ? QString{} : text.mid( slash );
	return result;
}

// Chrome/Edge/Coc Coc/Brave URL filter format: "host" includes sub domains, ".host" is the exact host,
// a path limits the filter to pages below it
QStringList chromiumFilters( const QStringList& entries )
{
	QStringList filters;
	for( const auto& entry : entries )
	{
		const auto website = parseEntry( entry );
		filters.append( ( website.exact ? QStringLiteral(".") : QString{} ) + website.host + website.path );
	}
	return filters;
}

QStringList firefoxPatterns( const QStringList& entries )
{
	QStringList patterns;
	for( const auto& entry : entries )
	{
		const auto website = parseEntry( entry );
		const auto path = website.path.isEmpty() ? QStringLiteral("/*") : website.path + QLatin1Char('*');
		patterns.append( QStringLiteral("*://%1%2").arg( website.host, path ) );
		if( website.exact == false )
		{
			patterns.append( QStringLiteral("*://*.%1%2").arg( website.host, path ) );
		}
	}
	return patterns;
}

}



QString RestrictionsBackend::webModeName( WebMode mode )
{
	switch( mode )
	{
	case WebMode::BlockAll: return QStringLiteral("BlockAll");
	case WebMode::AllowListed: return QStringLiteral("AllowListed");
	case WebMode::BlockListed: return QStringLiteral("BlockListed");
	case WebMode::None: break;
	}
	return {};
}



RestrictionsBackend::WebMode RestrictionsBackend::webModeFromName( const QString& name )
{
	for( const auto mode : { WebMode::BlockAll, WebMode::AllowListed, WebMode::BlockListed } )
	{
		if( webModeName( mode ) == name )
		{
			return mode;
		}
	}
	return WebMode::None;
}



QJsonObject RestrictionsBackend::loadState()
{
	QFile file( stateFilePath() );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return {};
	}
	return QJsonDocument::fromJson( file.readAll() ).object();
}



bool RestrictionsBackend::saveState( const QJsonObject& state )
{
	QDir().mkpath( QFileInfo( stateFilePath() ).absolutePath() );

	QSaveFile file( stateFilePath() );
	if( file.open( QFile::WriteOnly ) == false )
	{
		vCritical() << "could not write" << stateFilePath();
		return false;
	}
	file.write( QJsonDocument( state ).toJson() );
	return file.commit();
}



bool RestrictionsBackend::isAnyRestrictionActive( const QJsonObject& state )
{
	return state.contains( QStringLiteral("web") ) || state.contains( QStringLiteral("apps") );
}



RestrictionsBackend::WebMode RestrictionsBackend::activeWebMode( const QJsonObject& state )
{
	return webModeFromName( state.value( QStringLiteral("web") ).toObject().value( QStringLiteral("mode") ).toString() );
}



bool RestrictionsBackend::applyWebFilter( WebMode mode, const QStringList& websites, QJsonObject& state )
{
	if( mode == WebMode::None )
	{
		return removeWebFilter( state );
	}

	const auto hosts = normalizedWebsites( websites );

	QStringList chromiumBlock;
	QStringList chromiumAllow;
	QStringList firefoxBlock;
	QStringList firefoxExceptions;

	switch( mode )
	{
	case WebMode::BlockAll:
		chromiumBlock = QStringList{ QStringLiteral("*") };
		firefoxBlock = QStringList{ QStringLiteral("<all_urls>") };
		break;
	case WebMode::AllowListed:
		chromiumBlock = QStringList{ QStringLiteral("*") };
		chromiumAllow = chromiumFilters( hosts );
		firefoxBlock = QStringList{ QStringLiteral("<all_urls>") };
		firefoxExceptions = firefoxPatterns( hosts );
		break;
	case WebMode::BlockListed:
		chromiumBlock = chromiumFilters( hosts );
		firefoxBlock = firefoxPatterns( hosts );
		break;
	case WebMode::None:
		break;
	}

	auto backups = state.value( QStringLiteral("policyBackups") ).toObject();

	bool success = true;
	for( const auto& key : chromiumPolicyKeys() )
	{
		success &= writePolicyList( key + QStringLiteral("\\URLBlocklist"), chromiumBlock, backups );
		success &= writePolicyList( key + QStringLiteral("\\URLAllowlist"), chromiumAllow, backups );
	}
	success &= writePolicyList( firefoxWebsiteFilterKey + QStringLiteral("\\Block"), firefoxBlock, backups );
	success &= writePolicyList( firefoxWebsiteFilterKey + QStringLiteral("\\Exceptions"), firefoxExceptions, backups );

	state[QStringLiteral("policyBackups")] = backups;

	// browsers can be bypassed by other programs and portable browsers - for "block all"
	// additionally block web traffic in the Windows Firewall
	success &= setWebFirewallRules( mode == WebMode::BlockAll );

	state[QStringLiteral("web")] = QJsonObject{
		{ QStringLiteral("mode"), webModeName( mode ) },
		{ QStringLiteral("websites"), QJsonArray::fromStringList( hosts ) },
	};

	refreshPolicies();

	// pages which are already open keep their connections - close them so that the browser has
	// to connect again (which the firewall blocks immediately for "block all")
	resetBrowserConnections();

	return success;
}



bool RestrictionsBackend::removeWebFilter( QJsonObject& state )
{
	auto success = restorePolicyLists( state );
	success &= setWebFirewallRules( false );
	state.remove( QStringLiteral("web") );

	refreshPolicies();

	return success;
}



bool RestrictionsBackend::removeAllRestrictions( QJsonObject& state )
{
	const auto success = removeWebFilter( state );
	removeAppControl( state );

	state.remove( QStringLiteral("since") );
	state.remove( QStringLiteral("expires") );

	return success;
}



QStringList RestrictionsBackend::normalizedWebsites( const QStringList& websites )
{
	static const QRegularExpression validHost( QStringLiteral("^[\\w.-]+$"), QRegularExpression::UseUnicodePropertiesOption );
	static const QRegularExpression validPath( QStringLiteral("^/[^\\s\"<>]*$") );

	// canonical form "[=]host[/path]":
	//   "google.com"           -> the website and all its sub domains
	//   "chat.google.com"      -> only this sub domain (and its sub domains)
	//   "youtube.com/shorts"   -> only pages below this path
	//   "=www.google.com"      -> exactly this host (no sub domains), "=" = "exact address"
	QStringList entries;
	for( auto entry : websites )
	{
		entry = entry.trimmed();
		if( entry.isEmpty() || entry.startsWith( QLatin1Char('#') ) )
		{
			continue;
		}

		const auto exact = entry.startsWith( QLatin1Char('=') );
		if( exact )
		{
			entry = entry.mid( 1 ).trimmed();
		}

		const auto schemeEnd = entry.indexOf( QStringLiteral("://") );
		if( schemeEnd >= 0 )
		{
			entry = entry.mid( schemeEnd + 3 );
		}

		// query and fragment are never part of a filter
		const auto queryStart = entry.indexOf( QRegularExpression( QStringLiteral("[?#]") ) );
		if( queryStart >= 0 )
		{
			entry = entry.left( queryStart );
		}

		const auto slash = entry.indexOf( QLatin1Char('/') );
		auto host = ( slash < 0 ? entry : entry.left( slash ) ).toLower();
		auto path = slash < 0 ? QString{} : entry.mid( slash );

		const auto portStart = host.indexOf( QLatin1Char(':') );
		if( portStart >= 0 )
		{
			host = host.left( portStart );
		}

		while( host.startsWith( QLatin1String("*.") ) || host.startsWith( QLatin1Char('.') ) )
		{
			host = host.mid( host.startsWith( QLatin1Char('.') ) ? 1 : 2 );
		}

		// "www.youtube.com" means the whole website (also youtube.com, m.youtube.com),
		// unless the exact address was requested
		if( exact == false && host.startsWith( QLatin1String("www.") ) && host.count( QLatin1Char('.') ) >= 2 )
		{
			host = host.mid( 4 );
		}

		while( path.endsWith( QLatin1Char('/') ) )
		{
			path.chop( 1 );
		}

		if( host.isEmpty() || validHost.match( host ).hasMatch() == false ||
			( path.isEmpty() == false && validPath.match( path ).hasMatch() == false ) )
		{
			continue;
		}

		const auto canonical = ( exact ? QStringLiteral("=") : QString{} ) + host + path;
		if( entries.contains( canonical ) == false )
		{
			entries.append( canonical );
		}
	}

	return entries;
}



bool RestrictionsBackend::writePolicyList( const QString& keyPath, const QStringList& entries, QJsonObject& backups )
{
	QSettings key( registryPath( keyPath ), QSettings::Registry64Format );

	// remember the original list once, before ITLabControl changes it for the first time
	if( backups.contains( keyPath ) == false )
	{
		QJsonObject values;
		for( const auto& name : sortedValueNames( key ) )
		{
			values[name] = key.value( name ).toString();
		}
		backups[keyPath] = values;
	}

	QStringList allEntries;
	const auto original = backups.value( keyPath ).toObject();
	for( auto it = original.begin(); it != original.end(); ++it )
	{
		allEntries.append( it.value().toString() );
	}
	for( const auto& entry : entries )
	{
		if( allEntries.contains( entry ) == false )
		{
			allEntries.append( entry );
		}
	}

	key.remove( QString{} );
	for( int i = 0; i < allEntries.size(); ++i )
	{
		key.setValue( QString::number( i + 1 ), allEntries[i] );
	}
	key.sync();

	if( key.status() != QSettings::NoError )
	{
		vCritical() << "could not write" << keyPath;
		return false;
	}

	return true;
}



bool RestrictionsBackend::restorePolicyLists( QJsonObject& state )
{
	bool success = true;

	const auto backups = state.value( QStringLiteral("policyBackups") ).toObject();
	for( auto it = backups.begin(); it != backups.end(); ++it )
	{
		const auto& keyPath = it.key();
		const auto values = it.value().toObject();

		if( values.isEmpty() )
		{
			// the list did not exist before - delete the whole key
			const auto separator = keyPath.lastIndexOf( QLatin1Char('\\') );
			{
				QSettings parentKey( registryPath( keyPath.left( separator ) ), QSettings::Registry64Format );
				parentKey.remove( keyPath.mid( separator + 1 ) );
				parentKey.sync();
				success &= parentKey.status() == QSettings::NoError;
			}

			// remove parent keys which are empty now (e.g. "Policies\CocCoc\Browser") - browsers show
			// "managed by your organization" as long as such keys exist
			static const QString policiesRoot = QStringLiteral("SOFTWARE\\Policies");
			auto path = keyPath.left( separator );
			while( path.length() > policiesRoot.length() && path.startsWith( policiesRoot ) )
			{
				const auto parentSeparator = path.lastIndexOf( QLatin1Char('\\') );
				{
					QSettings key( registryPath( path ), QSettings::Registry64Format );
					if( key.childKeys().isEmpty() == false || key.childGroups().isEmpty() == false )
					{
						break;
					}
				}
				QSettings parentKey( registryPath( path.left( parentSeparator ) ), QSettings::Registry64Format );
				parentKey.remove( path.mid( parentSeparator + 1 ) );
				parentKey.sync();
				path = path.left( parentSeparator );
			}
		}
		else
		{
			QSettings key( registryPath( keyPath ), QSettings::Registry64Format );
			key.remove( QString{} );
			for( auto valueIt = values.begin(); valueIt != values.end(); ++valueIt )
			{
				key.setValue( valueIt.key(), valueIt.value().toString() );
			}
			key.sync();
			success &= key.status() == QSettings::NoError;
		}
	}

	if( success )
	{
		state.remove( QStringLiteral("policyBackups") );
	}
	else
	{
		vCritical() << "could not restore all browser policies - keeping the backup for the next attempt";
	}

	return success;
}



void RestrictionsBackend::refreshPolicies()
{
	// Chrome and Edge reload their policies when Windows announces a policy refresh;
	// Firefox reads its policies when it is started
	RefreshPolicyEx( TRUE, RP_FORCE );
}



void RestrictionsBackend::refreshUserPolicies()
{
	RefreshPolicyEx( FALSE, RP_FORCE );
}



bool RestrictionsBackend::setWebFirewallRules( bool enabled )
{
	const auto netsh = QStringLiteral("netsh");

	// always remove first so that the rules never exist twice
	for( const auto& name : firewallRuleNames )
	{
		runHidden( netsh, { QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("delete"),
							QStringLiteral("rule"), QStringLiteral("name=%1").arg( name ) } );
	}

	if( enabled == false )
	{
		return true;
	}

	bool success = runHidden( netsh, { QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
									   QStringLiteral("rule"), QStringLiteral("name=%1").arg( firewallRuleNames[0] ),
									   QStringLiteral("dir=out"), QStringLiteral("action=block"), QStringLiteral("protocol=TCP"),
									   QStringLiteral("remoteport=80,443"), QStringLiteral("enable=yes") } );
	// QUIC (HTTP/3)
	success &= runHidden( netsh, { QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"),
								   QStringLiteral("rule"), QStringLiteral("name=%1").arg( firewallRuleNames[1] ),
								   QStringLiteral("dir=out"), QStringLiteral("action=block"), QStringLiteral("protocol=UDP"),
								   QStringLiteral("remoteport=443"), QStringLiteral("enable=yes") } );
	return success;
}



bool RestrictionsBackend::runHidden( const QString& program, const QStringList& arguments )
{
	QProcess process;
	// never show a console window on the student's desktop
	process.setCreateProcessArgumentsModifier( []( QProcess::CreateProcessArguments* args ) {
		args->flags |= CREATE_NO_WINDOW;
	} );
	process.start( program, arguments );
	if( process.waitForFinished( 15000 ) == false )
	{
		vWarning() << program << arguments << "did not finish";
		return false;
	}
	return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}
