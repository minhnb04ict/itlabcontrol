/*
 * RemoteDirectoryDialog.cpp - browse folders of a student computer (ITLabControl)
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

#include <QDateTime>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "FileCollectController.h"
#include "RemoteDirectoryDialog.h"


static constexpr int PathRole = Qt::UserRole + 1;
static constexpr int IsDirRole = Qt::UserRole + 2;
static constexpr int ListingTimeout = 15000;


RemoteDirectoryDialog::RemoteDirectoryDialog(FileCollectController* controller,
											 ComputerControlInterface::Pointer computerControlInterface,
											 const QString& initialPath, QWidget* parent) :
	QDialog(parent),
	m_controller(controller),
	m_computerControlInterface(computerControlInterface),
	m_pathEdit(new QLineEdit(this)),
	m_upButton(new QToolButton(this)),
	m_homeButton(new QToolButton(this)),
	m_entryList(new QTreeWidget(this)),
	m_statusLabel(new QLabel(this)),
	m_chooseFolderButton(new QPushButton(tr("Collect this whole folder"), this)),
	m_chooseFilesButton(new QPushButton(tr("Collect selected files"), this))
{
	setWindowTitle(tr("Browse folders on %1").arg(computerControlInterface->computerName()));
	resize(760, 520);

	m_upButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogToParent));
	m_upButton->setToolTip(tr("Up one level"));
	m_homeButton->setIcon(style()->standardIcon(QStyle::SP_DirHomeIcon));
	m_homeButton->setToolTip(tr("User folders and drives"));
	m_pathEdit->setPlaceholderText(tr("User folders and drives"));

	auto pathLayout = new QHBoxLayout;
	pathLayout->addWidget(m_homeButton);
	pathLayout->addWidget(m_upButton);
	pathLayout->addWidget(m_pathEdit);

	m_entryList->setColumnCount(3);
	m_entryList->setHeaderLabels({tr("Name"), tr("Size"), tr("Modified")});
	m_entryList->setRootIsDecorated(false);
	m_entryList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_entryList->setSortingEnabled(false);
	m_entryList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	m_entryList->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	m_entryList->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

	auto hint = new QLabel(tr("Double-click a folder to open it. Hold Ctrl or Shift to select several files."), this);
	hint->setWordWrap(true);

	auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
	buttonBox->addButton(m_chooseFilesButton, QDialogButtonBox::AcceptRole);
	buttonBox->addButton(m_chooseFolderButton, QDialogButtonBox::AcceptRole);

	auto layout = new QVBoxLayout(this);
	layout->addLayout(pathLayout);
	layout->addWidget(m_entryList);
	layout->addWidget(m_statusLabel);
	layout->addWidget(hint);
	layout->addWidget(buttonBox);

	connect(m_controller, &FileCollectController::directoryListingReceived, this, &RemoteDirectoryDialog::handleListing);
	connect(m_upButton, &QToolButton::clicked, this, &RemoteDirectoryDialog::goUp);
	connect(m_homeButton, &QToolButton::clicked, this, [this]() { navigate({}); });
	connect(m_pathEdit, &QLineEdit::returnPressed, this, [this]() { navigate(m_pathEdit->text().trimmed()); });
	connect(m_entryList, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
		if (item->data(0, IsDirRole).toBool())
		{
			navigate(item->data(0, PathRole).toString());
		}
		else
		{
			chooseFiles();
		}
	});
	connect(m_entryList, &QTreeWidget::itemSelectionChanged, this, &RemoteDirectoryDialog::updateButtons);
	connect(m_chooseFolderButton, &QPushButton::clicked, this, &RemoteDirectoryDialog::chooseFolder);
	connect(m_chooseFilesButton, &QPushButton::clicked, this, &RemoteDirectoryDialog::chooseFiles);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	navigate(initialPath);
}



RemoteDirectoryDialog::~RemoteDirectoryDialog()
{
	m_controller->finishBrowsing(m_computerControlInterface, m_browseId);
}



void RemoteDirectoryDialog::navigate(const QString& path)
{
	m_requestedPath = path;
	m_entryList->setEnabled(false);
	m_statusLabel->setText(tr("Loading the folder from the student computer..."));
	updateButtons();

	m_controller->requestDirectoryListing(m_computerControlInterface, m_browseId, path);

	QTimer::singleShot(ListingTimeout, this, [this, path]() {
		if (m_requestedPath == path && m_entryList->isEnabled() == false)
		{
			m_entryList->setEnabled(true);
			m_statusLabel->setText(tr("The student computer did not answer. Please check that a user is logged on and try again."));
		}
	});
}



void RemoteDirectoryDialog::handleListing(ComputerControlInterface::Pointer computerControlInterface,
										  const QUuid& browseId, const QString& path,
										  const QVariantList& entries, const QString& errorString)
{
	if (computerControlInterface != m_computerControlInterface || browseId != m_browseId ||
		path != m_requestedPath)
	{
		return;
	}

	m_entryList->setEnabled(true);

	if (errorString.isEmpty() == false)
	{
		m_statusLabel->setText(errorString);
		updateButtons();
		return;
	}

	m_currentPath = path;
	m_pathEdit->setText(path);
	m_entryList->clear();

	int folderCount = 0;
	int fileCount = 0;
	for (const auto& entryValue : entries)
	{
		const auto entry = entryValue.toMap();
		const auto isDir = entry.value(QStringLiteral("dir")).toBool();

		auto item = new QTreeWidgetItem(m_entryList);
		item->setText(0, entry.value(QStringLiteral("name")).toString());
		item->setIcon(0, style()->standardIcon(isDir ? QStyle::SP_DirIcon : QStyle::SP_FileIcon));
		item->setData(0, PathRole, entry.value(QStringLiteral("path")));
		item->setData(0, IsDirRole, isDir);

		if (isDir)
		{
			++folderCount;
		}
		else
		{
			++fileCount;
			item->setText(1, QLocale().formattedDataSize(entry.value(QStringLiteral("size")).toLongLong()));
			item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
		}

		const auto modified = entry.value(QStringLiteral("modified")).toLongLong();
		if (modified > 0)
		{
			item->setText(2, QLocale().toString(QDateTime::fromMSecsSinceEpoch(modified), QLocale::ShortFormat));
		}
	}

	m_statusLabel->setText(path.isEmpty() ? tr("Choose a user folder or a drive.")
										  : tr("%1 folders, %2 files").arg(folderCount).arg(fileCount));
	updateButtons();
}



void RemoteDirectoryDialog::goUp()
{
	auto path = m_currentPath;
	while (path.endsWith(QLatin1Char('\\')) || path.endsWith(QLatin1Char('/')))
	{
		path.chop(1);
	}

	const auto separator = std::max(path.lastIndexOf(QLatin1Char('\\')), path.lastIndexOf(QLatin1Char('/')));
	if (path.isEmpty() || separator < 0)
	{
		// drive root (e.g. "C:") or unknown: back to the start page
		navigate({});
		return;
	}

	auto parentPath = path.left(separator);
	if (parentPath.endsWith(QLatin1Char(':')))
	{
		parentPath += QLatin1Char('\\');
	}
	navigate(parentPath);
}



void RemoteDirectoryDialog::updateButtons()
{
	const auto loaded = m_entryList->isEnabled();
	const auto inFolder = loaded && m_currentPath.isEmpty() == false;

	bool fileSelected = false;
	const auto selectedItems = m_entryList->selectedItems();
	for (const auto* item : selectedItems)
	{
		if (item->data(0, IsDirRole).toBool() == false)
		{
			fileSelected = true;
			break;
		}
	}

	m_upButton->setEnabled(inFolder);
	m_chooseFolderButton->setEnabled(inFolder);
	m_chooseFilesButton->setEnabled(inFolder && fileSelected);
}



void RemoteDirectoryDialog::chooseFolder()
{
	if (m_currentPath.isEmpty())
	{
		return;
	}

	m_selectedDirectory = m_currentPath;
	m_selectedFiles.clear();
	accept();
}



void RemoteDirectoryDialog::chooseFiles()
{
	if (m_currentPath.isEmpty())
	{
		return;
	}

	QStringList files;
	const auto selectedItems = m_entryList->selectedItems();
	for (const auto* item : selectedItems)
	{
		if (item->data(0, IsDirRole).toBool() == false)
		{
			files.append(item->text(0));
		}
	}

	if (files.isEmpty())
	{
		return;
	}

	m_selectedDirectory = m_currentPath;
	m_selectedFiles = files;
	accept();
}
