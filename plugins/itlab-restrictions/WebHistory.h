/*
 * WebHistory.h - keep the web browsing history of student computers for 30 days (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 *  The ITLabControl Server (SYSTEM) of a student computer regularly copies new entries of the history
 *  databases of Chrome, Edge, Coc Coc, Brave and Firefox of all users into
 *  %ProgramData%\ITLabControl\webhistory\<yyyy-MM-dd>.tsv. Students deleting the history of their
 *  browser does not delete these entries. Files older than ITLab/WebHistoryDays (default 30) days are
 *  deleted. Private/incognito windows are not stored by the browsers and therefore not available.
 */

#pragma once

#include <QVariantList>

namespace WebHistory {

int retentionDays();

// reads new visits from the browsers and removes old files; may take a moment (run in background)
void collect();

// entries as QVariantList{ msSinceEpoch, user, browser, url, title }, most recent last, at most maxEntries
QVariantList query( qint64 fromMs, qint64 toMs, int maxEntries );

}
