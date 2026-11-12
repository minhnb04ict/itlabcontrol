/*
 * RemoteDirectoryDialog.h - browse folders of a student computer (ITLabControl)
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
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include <QDialog>
#include <QUuid>

#include "ComputerControlInterface.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QTreeWidget;
class FileCollectController;

// Lets the teacher pick a folder or single files on one student computer. The listing is
// done by the worker in the session of the logged on user, i.e. with the student's permissions.
class RemoteDirectoryDialog : public QDialog
{
	Q_OBJECT
public:
	RemoteDirectoryDialog(FileCollectController* controller,
						  ComputerControlInterface::Pointer computerControlInterface,
						  const QString& initialPath, QWidget* parent);
	~RemoteDirectoryDialog() override;

	// folder to collect from (native separators)
	QString selectedDirectory() const
	{
		return m_selectedDirectory;
	}

	// chosen file names inside selectedDirectory(); empty = all files of the folder
	QStringList selectedFiles() const
	{
		return m_selectedFiles;
	}

private:
	void navigate(const QString& path);
	void handleListing(ComputerControlInterface::Pointer computerControlInterface, const QUuid& browseId,
					   const QString& path, const QVariantList& entries, const QString& errorString);
	void goUp();
	void updateButtons();
	void chooseFolder();
	void chooseFiles();

	FileCollectController* m_controller;
	ComputerControlInterface::Pointer m_computerControlInterface;
	const QUuid m_browseId{QUuid::createUuid()};

	QString m_currentPath;
	QString m_requestedPath;
	QString m_selectedDirectory;
	QStringList m_selectedFiles;

	QLineEdit* m_pathEdit;
	QToolButton* m_upButton;
	QToolButton* m_homeButton;
	QTreeWidget* m_entryList;
	QLabel* m_statusLabel;
	QPushButton* m_chooseFolderButton;
	QPushButton* m_chooseFilesButton;

};
