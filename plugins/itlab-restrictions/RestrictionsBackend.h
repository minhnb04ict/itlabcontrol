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

private:
	static bool writePolicyList( const QString& keyPath, const QStringList& entries, QJsonObject& backups );
	static bool restorePolicyLists( QJsonObject& state );
	static void refreshPolicies();
	static bool setWebFirewallRules( bool enabled );
	static bool runHidden( const QString& program, const QStringList& arguments );

};
