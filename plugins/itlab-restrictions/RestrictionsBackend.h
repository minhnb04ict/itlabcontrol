/*
 * RestrictionsBackend.h - applies and removes restrictions on a student computer (ITLabControl)
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

#pragma once

#include <QJsonObject>
#include <QStringList>

// Everything here runs with SYSTEM rights (ITLabControl Server) or as administrator (veyon-cli).
//
// Safety rules (see docs/ITLabControl_Design_Plan.md, phase 2, item 0):
// - the state is stored in %ProgramData%\ITLabControl\restrictions.json, so restrictions survive a
//   reboot and can always be removed again, also by "veyon-cli restrictions clear";
// - existing browser policies (e.g. set by the IT department) are backed up before ITLabControl adds
//   its entries and restored exactly when the restriction is removed;
// - every restriction expires (no heartbeat from the Teacher Console / maximum duration).
class RestrictionsBackend
{
public:
	enum class WebMode
	{
		None,
		BlockAll,       // no website at all
		AllowListed,    // only the listed websites
		BlockListed,    // everything except the listed websites
	};

	static QString webModeName( WebMode mode );
	static WebMode webModeFromName( const QString& name );

	// current state from the state file
	static QJsonObject loadState();
	static bool saveState( const QJsonObject& state );

	static bool isAnyRestrictionActive( const QJsonObject& state );
	static WebMode activeWebMode( const QJsonObject& state );

	// web filter: browser policies (Chrome, Edge, Coc Coc, Brave, Firefox) + firewall for "block all"
	static bool applyWebFilter( WebMode mode, const QStringList& websites, QJsonObject& state );
	static bool removeWebFilter( QJsonObject& state );

	// removes everything ITLabControl has applied and restores the backed up policies
	static bool removeAllRestrictions( QJsonObject& state );

	// "vnexpress.net", "https://www.Example.com/abc" -> host names in lower case
	static QStringList normalizedWebsites( const QStringList& websites );

	// closes the open web connections of browsers so that new restrictions take effect
	// immediately for pages which are already open (IPv4 connections on port 80/443)
	static void resetBrowserConnections();

	// called in the session of the student: browsers also reload their policies on user policy refreshes
	static void refreshUserPolicies();

	// ── applications ──────────────────────────────────────────────────────────
	enum class AppMode
	{
		None,
		BlockListed,    // the listed programs are closed as soon as they are started
		AllowListed,    // only the listed programs (and Windows itself) may show windows
	};

	static QString appModeName( AppMode mode );
	static AppMode appModeFromName( const QString& name );
	static AppMode activeAppMode( const QJsonObject& state );
	static QStringList activeApps( const QJsonObject& state );

	static void applyAppControl( AppMode mode, const QStringList& apps, QJsonObject& state );
	static void removeAppControl( QJsonObject& state );

	// "C:\Games\Game.EXE", "zalo" -> "game.exe", "zalo.exe"
	static QStringList normalizedApps( const QStringList& apps );

	// closes programs of the current Windows session which are not allowed;
	// returns the names of the closed programs
	static QStringList enforceAppControl( AppMode mode, const QStringList& apps );

	static const QStringList& browserProcessNames();

private:
	static bool writePolicyList( const QString& keyPath, const QStringList& entries, QJsonObject& backups );
	static bool restorePolicyLists( QJsonObject& state );
	static void refreshPolicies();
	static bool setWebFirewallRules( bool enabled );
	static bool runHidden( const QString& program, const QStringList& arguments );

};
