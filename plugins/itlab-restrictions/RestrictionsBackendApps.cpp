/*
 * RestrictionsBackendApps.cpp - application control and immediate effect for open web pages (ITLabControl)
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
#include <iphlpapi.h>
#include <tlhelp32.h>

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

#include "RestrictionsBackend.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"


// ═════════════════════════════════════════════════════════════════════════════
//  immediate effect for open pages
// ═════════════════════════════════════════════════════════════════════════════

const QStringList& RestrictionsBackend::browserProcessNames()
{
	static const QStringList names{
		QStringLiteral("chrome.exe"), QStringLiteral("msedge.exe"), QStringLiteral("firefox.exe"),
		QStringLiteral("browser.exe") /* Coc Coc */, QStringLiteral("brave.exe"), QStringLiteral("opera.exe"),
		QStringLiteral("vivaldi.exe"), QStringLiteral("iexplore.exe"),
	};
	return names;
}



void RestrictionsBackend::resetBrowserConnections()
{
	QSet<DWORD> browserPids;

	const auto snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( snapshot == INVALID_HANDLE_VALUE )
	{
		return;
	}
	PROCESSENTRY32W entry{};
	entry.dwSize = sizeof(entry);
	for( auto ok = Process32FirstW( snapshot, &entry ); ok; ok = Process32NextW( snapshot, &entry ) )
	{
		if( browserProcessNames().contains( QString::fromWCharArray( entry.szExeFile ).toLower() ) )
		{
			browserPids.insert( entry.th32ProcessID );
		}
	}
	CloseHandle( snapshot );

	if( browserPids.isEmpty() )
	{
		return;
	}

	ULONG size = 0;
	GetExtendedTcpTable( nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0 );
	QByteArray buffer( int(size), 0 );
	if( GetExtendedTcpTable( buffer.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_CONNECTIONS, 0 ) != NO_ERROR )
	{
		return;
	}

	int resetCount = 0;
	const auto table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>( buffer.data() );
	for( DWORD i = 0; i < table->dwNumEntries; ++i )
	{
		const auto& row = table->table[i];
		const auto remotePort = ntohs( u_short( row.dwRemotePort ) );
		if( row.dwState != MIB_TCP_STATE_ESTAB || browserPids.contains( row.dwOwningPid ) == false ||
			( remotePort != 80 && remotePort != 443 ) )
		{
			continue;
		}

		MIB_TCPROW tcpRow{};
		tcpRow.dwState = MIB_TCP_STATE_DELETE_TCB;
		tcpRow.dwLocalAddr = row.dwLocalAddr;
		tcpRow.dwLocalPort = row.dwLocalPort;
		tcpRow.dwRemoteAddr = row.dwRemoteAddr;
		tcpRow.dwRemotePort = row.dwRemotePort;
		if( SetTcpEntry( &tcpRow ) == NO_ERROR )
		{
			++resetCount;
		}
	}

	vDebug() << "closed" << resetCount << "browser connections";
}



// ═════════════════════════════════════════════════════════════════════════════
//  applications
// ═════════════════════════════════════════════════════════════════════════════

QString RestrictionsBackend::appModeName( AppMode mode )
{
	switch( mode )
	{
	case AppMode::BlockListed: return QStringLiteral("BlockListed");
	case AppMode::AllowListed: return QStringLiteral("AllowListed");
	case AppMode::None: break;
	}
	return {};
}



RestrictionsBackend::AppMode RestrictionsBackend::appModeFromName( const QString& name )
{
	for( const auto mode : { AppMode::BlockListed, AppMode::AllowListed } )
	{
		if( appModeName( mode ) == name )
		{
			return mode;
		}
	}
	return AppMode::None;
}



RestrictionsBackend::AppMode RestrictionsBackend::activeAppMode( const QJsonObject& state )
{
	return appModeFromName( state.value( QStringLiteral("apps") ).toObject().value( QStringLiteral("mode") ).toString() );
}



QStringList RestrictionsBackend::activeApps( const QJsonObject& state )
{
	QStringList apps;
	const auto array = state.value( QStringLiteral("apps") ).toObject().value( QStringLiteral("apps") ).toArray();
	for( const auto& app : array )
	{
		apps.append( app.toString() );
	}
	return apps;
}



void RestrictionsBackend::applyAppControl( AppMode mode, const QStringList& apps, QJsonObject& state )
{
	if( mode == AppMode::None )
	{
		removeAppControl( state );
		return;
	}

	state[QStringLiteral("apps")] = QJsonObject{
		{ QStringLiteral("mode"), appModeName( mode ) },
		{ QStringLiteral("apps"), QJsonArray::fromStringList( normalizedApps( apps ) ) },
	};
}



void RestrictionsBackend::removeAppControl( QJsonObject& state )
{
	state.remove( QStringLiteral("apps") );
}



QStringList RestrictionsBackend::normalizedApps( const QStringList& apps )
{
	QStringList names;
	for( auto entry : apps )
	{
		entry = entry.trimmed().toLower();
		if( entry.isEmpty() || entry.startsWith( QLatin1Char('#') ) )
		{
			continue;
		}
		entry = entry.mid( std::max( entry.lastIndexOf( QLatin1Char('\\') ), entry.lastIndexOf( QLatin1Char('/') ) ) + 1 );
		if( entry.contains( QLatin1Char('.') ) == false )
		{
			entry += QStringLiteral(".exe");
		}
		if( entry.isEmpty() == false && names.contains( entry ) == false )
		{
			names.append( entry );
		}
	}
	return names;
}



namespace {

BOOL CALLBACK collectWindowProcess( HWND window, LPARAM parameter )
{
	// main windows of applications: visible, not owned by another window, with a title
	if( IsWindowVisible( window ) && GetWindow( window, GW_OWNER ) == nullptr && GetWindowTextLengthW( window ) > 0 )
	{
		DWORD pid = 0;
		GetWindowThreadProcessId( window, &pid );
		reinterpret_cast<QSet<DWORD> *>( parameter )->insert( pid );
	}
	return TRUE;
}

QString processImagePath( HANDLE process )
{
	wchar_t path[MAX_PATH * 2]{};
	DWORD length = MAX_PATH * 2;
	if( QueryFullProcessImageNameW( process, 0, path, &length ) )
	{
		return QString::fromWCharArray( path, int(length) ).toLower();
	}
	return {};
}

}



QStringList RestrictionsBackend::enforceAppControl( AppMode mode, const QStringList& apps )
{
	QStringList closed;
	if( mode == AppMode::None )
	{
		return closed;
	}

	DWORD ownSession = 0;
	if( ProcessIdToSessionId( GetCurrentProcessId(), &ownSession ) == FALSE || ownSession == 0 )
	{
		return closed; // only programs of a logged on user are handled
	}

	const QSet<QString> listed( apps.begin(), apps.end() );

	// never closed: Vietnamese input methods and the applications protected by the IT staff
	QSet<QString> alwaysAllowed{
		QStringLiteral("unikeynt.exe"), QStringLiteral("unikey.exe"), QStringLiteral("evkey.exe"),
		QStringLiteral("evkey32.exe"), QStringLiteral("evkey64.exe"), QStringLiteral("openkey.exe"),
	};
	const auto protectedApps = VeyonCore::config().value( QStringLiteral("ProtectedApplications"),
														  QStringLiteral("ClearDesktop"), QString{} ).toString()
								   .toLower().split( QRegularExpression( QStringLiteral("[;,\\s]+") ), Qt::SkipEmptyParts );
	for( const auto& app : protectedApps )
	{
		alwaysAllowed.insert( app );
	}

	const auto systemRoot = QDir::toNativeSeparators( qEnvironmentVariable( "SystemRoot", QStringLiteral("C:\\Windows") ) ).toLower() + QLatin1Char('\\');
	const auto ownDirectory = QDir::toNativeSeparators( QCoreApplication::applicationDirPath() ).toLower() + QLatin1Char('\\');

	QSet<DWORD> windowProcesses;
	if( mode == AppMode::AllowListed )
	{
		EnumWindows( collectWindowProcess, reinterpret_cast<LPARAM>( &windowProcesses ) );
	}

	const auto snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( snapshot == INVALID_HANDLE_VALUE )
	{
		return closed;
	}

	PROCESSENTRY32W entry{};
	entry.dwSize = sizeof(entry);
	for( auto ok = Process32FirstW( snapshot, &entry ); ok; ok = Process32NextW( snapshot, &entry ) )
	{
		const auto pid = entry.th32ProcessID;
		DWORD session = 0;
		if( pid <= 4 || pid == GetCurrentProcessId() ||
			ProcessIdToSessionId( pid, &session ) == FALSE || session != ownSession )
		{
			continue;
		}

		const auto name = QString::fromWCharArray( entry.szExeFile ).toLower();
		if( alwaysAllowed.contains( name ) )
		{
			continue;
		}

		if( mode == AppMode::BlockListed && listed.contains( name ) == false )
		{
			continue;
		}
		if( mode == AppMode::AllowListed && ( listed.contains( name ) || windowProcesses.contains( pid ) == false ) )
		{
			continue;
		}

		const auto process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, pid );
		if( process == nullptr )
		{
			continue;
		}

		// never close ITLabControl itself; in "allow only" mode never close programs of Windows
		const auto path = processImagePath( process );
		const auto keep = path.isEmpty() || path.startsWith( ownDirectory ) ||
						  ( mode == AppMode::AllowListed && path.startsWith( systemRoot ) );
		if( keep == false && TerminateProcess( process, 1 ) )
		{
			closed.append( name );
		}
		CloseHandle( process );
	}
	CloseHandle( snapshot );

	return closed;
}
