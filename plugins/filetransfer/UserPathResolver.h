/*
 * UserPathResolver.h - resolve paths entered by the teacher on a student computer (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QDir>
#include <QMap>
#include <QStandardPaths>

#include "Filesystem.h"
#include "VeyonCore.h"

namespace UserPathResolver {

// Runs in the session of the student (worker), so the folders of the logged on user are used.
//   "Desktop\BaiTap", "Màn hình nền\BaiTap" -> real desktop folder (also when redirected to OneDrive)
//   "Documents", "Downloads", "Pictures", ... (English or Vietnamese names)
//   "%DOCUMENTS%\Lop10"                     -> placeholders of ITLabControl
//   "D:\BaiLam\bai1.docx"                   -> absolute paths unchanged
//   "BaiTap"                                -> folder in the profile of the user
inline QString resolve( const QString& path )
{
	auto result = VeyonCore::filesystem().expandPath( path.trimmed() );
	if( result.isEmpty() )
	{
		return {};
	}

	result = QDir::fromNativeSeparators( result );
	if( QDir::isAbsolutePath( result ) )
	{
		return QDir::toNativeSeparators( QDir::cleanPath( result ) );
	}

	static const QMap<QString, QStandardPaths::StandardLocation> knownFolders{
		{ QStringLiteral("desktop"), QStandardPaths::DesktopLocation },
		{ QStringLiteral("màn hình nền"), QStandardPaths::DesktopLocation },
		{ QStringLiteral("documents"), QStandardPaths::DocumentsLocation },
		{ QStringLiteral("my documents"), QStandardPaths::DocumentsLocation },
		{ QStringLiteral("tài liệu"), QStandardPaths::DocumentsLocation },
		{ QStringLiteral("downloads"), QStandardPaths::DownloadLocation },
		{ QStringLiteral("tải về"), QStandardPaths::DownloadLocation },
		{ QStringLiteral("pictures"), QStandardPaths::PicturesLocation },
		{ QStringLiteral("hình ảnh"), QStandardPaths::PicturesLocation },
		{ QStringLiteral("music"), QStandardPaths::MusicLocation },
		{ QStringLiteral("nhạc"), QStandardPaths::MusicLocation },
		{ QStringLiteral("videos"), QStandardPaths::MoviesLocation },
		{ QStringLiteral("video"), QStandardPaths::MoviesLocation },
	};

	const auto separator = result.indexOf( QLatin1Char('/') );
	const auto first = ( separator < 0 ? result : result.left( separator ) ).trimmed().toLower();
	const auto rest = separator < 0 ? QString{} : result.mid( separator + 1 );

	const auto known = knownFolders.constFind( first );
	if( known != knownFolders.constEnd() )
	{
		const auto base = QStandardPaths::writableLocation( known.value() );
		if( base.isEmpty() == false )
		{
			return QDir::toNativeSeparators( QDir::cleanPath( base + QLatin1Char('/') + rest ) );
		}
	}

	return QDir::toNativeSeparators( QDir::cleanPath( QDir::homePath() + QLatin1Char('/') + result ) );
}

// "Folder/sub/a.txt" received from the teacher computer -> safe relative path ("..", drive letters
// and absolute paths are removed so that files are never written outside the destination folder)
inline QString safeRelativePath( const QString& path )
{
	QStringList parts;
	const auto segments = QDir::fromNativeSeparators( path ).split( QLatin1Char('/'), Qt::SkipEmptyParts );
	for( const auto& segment : segments )
	{
		const auto trimmed = segment.trimmed();
		if( trimmed.isEmpty() || trimmed == QLatin1String(".") || trimmed == QLatin1String("..") ||
			trimmed.contains( QLatin1Char(':') ) )
		{
			continue;
		}
		parts.append( trimmed );
	}
	return parts.join( QDir::separator() );
}

}
