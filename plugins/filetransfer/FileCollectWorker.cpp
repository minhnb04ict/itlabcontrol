/*
 * FileCollectWorker.cpp - implementation of FileCollectWorker class
 *
 * Copyright (c) 2025-2026 Tobias Junghans <tobydox@veyon.io>
 *
 * This file is part of Veyon - https://veyon.io
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
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

#include "FileCollectWorker.h"
#include "FileTransferPlugin.h"
#include "Filesystem.h"
#include "UserPathResolver.h"


static QStringList listFilesInDirectory(const QString& dirPath, const QList<QRegularExpression>& excludeRegExes, bool recursive = false)
{
	QStringList fileList;
	QDir dir(dirPath);

	if (!dir.exists())
		return fileList;

	const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
	fileList.reserve(entries.size());

	for (const QFileInfo& fi : entries)
	{
		if (fi.isReadable() == false)
		{
			vDebug() << "skipping non-readable file" << fi.fileName();
			continue;
		}

		bool exclude = false;
		for (const auto& excludeRegEx : excludeRegExes)
		{
			if (excludeRegEx.match(fi.fileName()).hasMatch())
			{
				vDebug() << "excluding" << fi.fileName() << "as it matches" << excludeRegEx;
				exclude = true;
				break;
			}
		}

		if (exclude == false)
		{
			fileList.append(QDir::toNativeSeparators(fi.absoluteFilePath()));
		}
	}

	if (recursive)
	{
		const QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
		for (const QFileInfo& subdirInfo : subdirs)
		{
			fileList.append(listFilesInDirectory(subdirInfo.absoluteFilePath(), excludeRegExes, true));
		}
	}

	return fileList;
}



FileCollectWorker::FileCollectWorker(FileTransferPlugin* plugin,
									 const QString& sourceDirectory,
									 const QString& filePattern,
									 bool collectRecursively) :
	QObject(plugin),
	m_configuration(plugin->configuration()),
	// ITLabControl: "Desktop\BaiTap", "%DOCUMENTS%", "D:\BaiLam" etc. are resolved to the real
	// (e.g. OneDrive-redirected) folders of the logged on user, see UserPathResolver
	m_sourceDirectory(UserPathResolver::resolve(sourceDirectory.isEmpty() ? m_configuration.filesToCollectSourceDirectory()
																		 : sourceDirectory)),
	m_filePattern(filePattern),
	m_collectRecursively(collectRecursively || m_configuration.collectFilesRecursively())
{
	// ITLabControl: the teacher may also enter the path of a single file
	const QFileInfo sourceInfo(m_sourceDirectory);
	if (sourceInfo.isFile())
	{
		m_singleFile = sourceInfo.fileName();
		m_sourceDirectory = QDir::toNativeSeparators(sourceInfo.absolutePath());
	}

	if (m_sourceDirectory.endsWith(QDir::separator()) == false)
	{
		m_sourceDirectory.append(QDir::separator());
	}

	initFiles();
}



FileCollectWorker::~FileCollectWorker()
{
}



QVariantList FileCollectWorker::fileSizes() const
{
	QVariantList sizes;
	sizes.reserve(m_files.size());
	for (const auto& file : m_files)
	{
		sizes.append(QFileInfo(m_sourceDirectory + file).size());
	}
	return sizes;
}



QVariantList FileCollectWorker::fileModificationTimes() const
{
	QVariantList times;
	times.reserve(m_files.size());
	for (const auto& file : m_files)
	{
		times.append(QFileInfo(m_sourceDirectory + file).lastModified().toMSecsSinceEpoch());
	}
	return times;
}



void FileCollectWorker::selectFiles(const QStringList& files)
{
	if (m_currentFileIndex >= 0)
	{
		vWarning() << "can't change file selection after transfers have started";
		return;
	}

	const QSet<QString> selected(files.begin(), files.end());
	QStringList remainingFiles;
	for (const auto& file : std::as_const(m_files))
	{
		if (selected.contains(file))
		{
			remainingFiles.append(file);
		}
	}
	m_files = remainingFiles;
}



QVariantList FileCollectWorker::listDirectory(const QString& path, QString* errorString, bool* pathIsFile)
{
	QVariantList entries;

	const auto makeEntry = [](const QString& name, const QString& entryPath, bool isDir, qint64 size, qint64 modified) {
		return QVariantMap{
			{ QStringLiteral("name"), name },
			{ QStringLiteral("path"), QDir::toNativeSeparators(entryPath) },
			{ QStringLiteral("dir"), isDir },
			{ QStringLiteral("size"), size },
			{ QStringLiteral("modified"), modified },
		};
	};

	if (path.isEmpty())
	{
		// starting points: personal folders of the logged on user, then all drives.
		// ITLabControl: personal folders get short paths ("Desktop", ...) which every student
		// computer resolves for its own user, so a path chosen on one computer works on all
		struct UserFolder { QString label; QString shortPath; QStandardPaths::StandardLocation location; };
		const QList<UserFolder> userFolders{
			{ tr("Documents"), QStringLiteral("Documents"), QStandardPaths::DocumentsLocation },
			{ tr("Desktop"), QStringLiteral("Desktop"), QStandardPaths::DesktopLocation },
			{ tr("Downloads"), QStringLiteral("Downloads"), QStandardPaths::DownloadLocation },
			{ tr("User folder"), QStringLiteral("."), QStandardPaths::HomeLocation },
		};
		for (const auto& folder : userFolders)
		{
			const auto folderPath = QStandardPaths::writableLocation(folder.location);
			if (folderPath.isEmpty() == false && QDir(folderPath).exists())
			{
				entries.append(makeEntry(folder.label, folder.shortPath, true, 0, 0));
			}
		}

		const auto drives = QDir::drives();
		for (const auto& drive : drives)
		{
			entries.append(makeEntry(QDir::toNativeSeparators(drive.absoluteFilePath()), drive.absoluteFilePath(), true, 0, 0));
		}

		return entries;
	}

	// ITLabControl: same path rules as for collecting; for a file the folder containing it is shown
	auto resolvedPath = UserPathResolver::resolve(path);
	if (QFileInfo(resolvedPath).isFile())
	{
		resolvedPath = QFileInfo(resolvedPath).absolutePath();
		if (pathIsFile)
		{
			*pathIsFile = true;
		}
	}
	const QDir dir(QDir::fromNativeSeparators(resolvedPath));
	if (dir.exists() == false)
	{
		if (errorString)
		{
			*errorString = tr("The folder %1 does not exist.").arg(path);
		}
		return entries;
	}

	const auto infos = dir.entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable,
										 QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
	for (const auto& info : infos)
	{
		if (info.isHidden())
		{
			continue;
		}
		entries.append(makeEntry(info.fileName(), info.absoluteFilePath(), info.isDir(),
								 info.isDir() ? 0 : info.size(), info.lastModified().toMSecsSinceEpoch()));
	}

	return entries;
}



QPair<FileCollectWorker::TransferState, QString> FileCollectWorker::startNextTransfer()
{
	const auto newFileIndex = m_currentFileIndex + 1;

	if (newFileIndex >= m_files.count())
	{
		m_currentTransferId = FileCollection::TransferId{};
		return {TransferState::AllFinished, {}};
	}

	m_currentTransferId = QUuid::createUuid();
	m_currentFile.close();
	m_currentFile.setFileName(m_sourceDirectory + std::as_const(m_files)[newFileIndex]);
	if (m_currentFile.open(QFile::ReadOnly))
	{
		m_currentFileIndex = newFileIndex;
		return {TransferState::Started, m_currentFile.fileName()};
	}

	return {TransferState::WaitingForLockedFile, m_currentFile.fileName()};
}



void FileCollectWorker::skipToNextFile()
{
	m_currentFileIndex++;
}



void FileCollectWorker::cancelCurrentTransfer()
{
	m_currentFile.close();
	m_currentTransferId = FileCollection::TransferId{};
}



QByteArray FileCollectWorker::readNextChunk()
{
	return m_currentFile.read(ChunkSize);
}



bool FileCollectWorker::currentFileAtEnd() const
{
	return m_currentFile.atEnd();
}



void FileCollectWorker::initFiles()
{
	if (m_singleFile.isEmpty() == false)
	{
		// a single file was chosen explicitly - collect it regardless of file type or exclude list
		m_files = QStringList{m_singleFile};
		return;
	}

	m_files = listFilesInDirectory(m_sourceDirectory, excludeRegExes(), m_collectRecursively);

	m_files = filterFilesByPattern(m_files, m_filePattern);

	for (auto it = m_files.begin(); it != m_files.end(); ) // clazy:exclude=detaching-member
	{
#ifdef Q_OS_WIN
		const auto caseSensitivity = Qt::CaseInsensitive;
#else
		const auto caseSensitivity = Qt::CaseSensitive;
#endif
		if (it->startsWith(m_sourceDirectory, caseSensitivity))
		{
			*it = it->mid(m_sourceDirectory.size());
			++it;
		}
		else
		{
			it = m_files.erase(it);
		}
	}

	// move files which currently can't be opened (i.e. locked files) to the end so that
	// all readable files are collected first
	std::stable_partition(m_files.begin(), m_files.end(), [&](const QString& fileName) {
		return QFile(m_sourceDirectory + fileName).open(QFile::ReadOnly);
	});
}



QList<QRegularExpression> FileCollectWorker::excludeRegExes() const
{
	const auto excludePatterns = m_configuration.filesToExcludeFromCollecting().replace(u';', u' ')
								 .replace(u',', u' ').split(u' ', Qt::SkipEmptyParts);

	QList<QRegularExpression> regExes;
	std::transform(excludePatterns.begin(), excludePatterns.end(),
				   std::back_inserter(regExes),
				   [](const QString& pattern) {
			return QRegularExpression(QString(pattern).replace(u'*', QStringLiteral(".*")),
									  QRegularExpression::CaseInsensitiveOption);
	});

	return regExes;
}



QStringList FileCollectWorker::filterFilesByPattern(const QStringList& files, const QString& pattern) const
{
	if (pattern.isEmpty())
	{
		return files;
	}

	auto patternStr = pattern;
	const auto globPatterns = patternStr.replace(u';', u' ')
								       .replace(u',', u' ').split(u' ', Qt::SkipEmptyParts);
	if (globPatterns.isEmpty())
	{
		return files;
	}

	QList<QRegularExpression> regExes;
	std::transform(globPatterns.begin(), globPatterns.end(),
				   std::back_inserter(regExes),
				   [](const QString& glob) {
		// ITLabControl: match whole file names ("*.doc" must not match "x.docx", "." is literal)
		return QRegularExpression(QRegularExpression::wildcardToRegularExpression(
									  glob, QRegularExpression::NonPathWildcardConversion),
								  QRegularExpression::CaseInsensitiveOption);
	});

	QStringList filtered;
	filtered.reserve(files.size());

	for (const auto& file : files)
	{
		const auto fileName = QFileInfo(file).fileName();
		bool matched = false;
		for (const auto& re : regExes)
		{
			if (re.match(fileName).hasMatch())
			{
				matched = true;
				break;
			}
		}
		if (matched)
		{
			filtered.append(file);
		}
	}

	return filtered;
}
