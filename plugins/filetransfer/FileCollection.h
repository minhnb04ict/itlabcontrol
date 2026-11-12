/*
 * FileCollection.h - declaration of FileCollection class
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

#pragma once

#include <QFile>

struct FileCollection {
	enum class State {
		Invalid,
		Initializing,
		ReadyForNextFileTransfer,
		FileTransferRunning,
		Finished,
		// ITLabControl: file list received, teacher chooses the files before collecting
		Previewed
	};

	using Id = QUuid;
	using TransferId = QUuid;

	Id id = QUuid::createUuid();

	State state = State::Invalid;

	QString name = {};
	QStringList files = {};
	// ITLabControl: details for the preview (same order as files; may be empty with older clients)
	QList<qint64> fileSizes = {};
	QList<qint64> fileModificationTimes = {}; // ms since epoch
	QList<bool> fileChecked = {};
	int processedFilesCount = 0;

	TransferId currentTransferId = {};
	QFile* currentOutputFile = nullptr;
	qint64 currentFileSize = 0;

	bool operator!=(const FileCollection& other) const
	{
		return other.id != id;
	}

	bool isFileChecked(int index) const
	{
		return fileChecked.value(index, true);
	}

	QStringList checkedFiles() const
	{
		QStringList result;
		for (int i = 0; i < files.size(); ++i)
		{
			if (isFileChecked(i))
			{
				result.append(files[i]);
			}
		}
		return result;
	}

	int checkedFilesCount() const
	{
		int count = 0;
		for (int i = 0; i < files.size(); ++i)
		{
			count += isFileChecked(i) ? 1 : 0;
		}
		return count;
	}

	qint64 checkedFilesSize() const
	{
		qint64 size = 0;
		for (int i = 0; i < files.size(); ++i)
		{
			if (isFileChecked(i))
			{
				size += fileSizes.value(i, 0);
			}
		}
		return size;
	}

	// keeps only the given files (in their current order) together with their details
	void restrictToFiles(const QStringList& selectedFiles)
	{
		QStringList newFiles;
		QList<qint64> newSizes;
		QList<qint64> newTimes;
		for (int i = 0; i < files.size(); ++i)
		{
			if (selectedFiles.contains(files[i]))
			{
				newFiles.append(files[i]);
				newSizes.append(fileSizes.value(i, 0));
				newTimes.append(fileModificationTimes.value(i, 0));
			}
		}
		files = newFiles;
		fileSizes = newSizes;
		fileModificationTimes = newTimes;
		fileChecked.clear();
	}

	int currentFileProgress() const
	{
		return currentOutputFile && currentFileSize > 0 ?
					(currentOutputFile->size() * 100 / currentFileSize)
				  :
					0;
	}

	int progress() const
	{
		if (files.isEmpty())
		{
			return state == State::Finished ? 100 : 0;
		}
		if (processedFilesCount >= files.size())
		{
			return 100;
		}
		return processedFilesCount * 100 / files.size() +
				currentFileProgress() / files.size();
	}
};
