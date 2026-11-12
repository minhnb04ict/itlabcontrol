/*
 * FileCollectDialog.cpp - implementation of FileCollectDialog
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

#include <QDesktopServices>
#include <QDir>
#include <QLocale>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>

#include "FileCollectController.h"
#include "FileCollectDialog.h"
#include "FileCollectTreeModel.h"
#include "FileSystemBrowser.h"
#include "Filesystem.h"
#include "ProgressItemDelegate.h"
#include "RemoteDirectoryDialog.h"

#include "ui_FileCollectDialog.h"


// ITLabControl: preset choices so teachers do not have to type paths on the student computers.
// The value (item data) is sent to the student computer, where placeholders like %DOCUMENTS%
// are resolved to the real folders of the logged on user (see FileCollectWorker).
static void populateComboBox(QComboBox* comboBox, const QList<QPair<QString, QString>>& items, const QString& value)
{
	for (const auto& item : items)
	{
		comboBox->addItem(item.first, item.second);
	}

	const auto index = comboBox->findData(value);
	if (index >= 0)
	{
		comboBox->setCurrentIndex(index);
	}
	else if (value.isEmpty())
	{
		comboBox->setCurrentIndex(0);
	}
	else
	{
		comboBox->setEditText(value);
	}
}



// Returns the value of the selected preset or the text entered by the user
static QString comboBoxValue(const QComboBox* comboBox)
{
	const auto index = comboBox->currentIndex();
	if (index >= 0 && comboBox->currentText() == comboBox->itemText(index))
	{
		return comboBox->itemData(index).toString();
	}

	return comboBox->currentText().trimmed();
}



FileCollectDialog::FileCollectDialog(FileCollectController* controller, QWidget* parent) :
	QDialog(parent),
	ui(new Ui::FileCollectDialog),
	m_controller(controller),
	m_model(new FileCollectTreeModel(controller, this))
{
	ui->setupUi(this);
	ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Start"));

	ui->collectionsTreeView->setItemDelegateForColumn(int(FileCollectTreeModel::Column::Progress),
													  new ProgressItemDelegate(ui->collectionsTreeView));
	ui->collectionsTreeView->setModel(m_model);

	// Initialize source directory from controller / default config
	populateComboBox(ui->sourceDirectoryComboBox, {
						 { tr("Documents folder of the user"), QStringLiteral("%DOCUMENTS%") },
						 { tr("Desktop of the user"), QStringLiteral("%DESKTOP%") },
						 { tr("Downloads folder of the user"), QStringLiteral("%DOWNLOADS%") },
						 { tr("Shared assignments folder (C:\\BaiLam)"), QStringLiteral("C:\\BaiLam") },
					 },
					 m_controller->collectSourceDirectory());

	// Initialize file pattern
	populateComboBox(ui->filePatternComboBox, {
						 { tr("All files"), QString() },
						 { tr("Word documents (*.doc, *.docx)"), QStringLiteral("*.doc;*.docx") },
						 { tr("Excel spreadsheets (*.xls, *.xlsx)"), QStringLiteral("*.xls;*.xlsx") },
						 { tr("PowerPoint presentations (*.ppt, *.pptx)"), QStringLiteral("*.ppt;*.pptx") },
						 { tr("PDF documents (*.pdf)"), QStringLiteral("*.pdf") },
						 { tr("Images (*.png, *.jpg, *.jpeg, *.bmp, *.gif)"), QStringLiteral("*.png;*.jpg;*.jpeg;*.bmp;*.gif") },
						 { tr("Text files (*.txt)"), QStringLiteral("*.txt") },
					 },
					 m_controller->filePattern());

	// Initialize destination directory
	ui->destinationDirectoryEdit->setText(m_controller->destinationDirectory());

	// ITLabControl: a file selection from the folder browser belongs to an earlier collect action
	m_controller->setFileSelection({});

	// ITLabControl: query the file lists first and let the teacher untick files which are not needed
	m_previewButton = ui->buttonBox->addButton(tr("Preview files"), QDialogButtonBox::ActionRole);
	m_previewButton->setToolTip(tr("Show the files which would be collected from each computer. "
								   "Untick the files you do not need, then click \"Start\"."));
	connect(m_previewButton, &QPushButton::clicked, this, &FileCollectDialog::preview);

	// ITLabControl: browsing is only possible for a single student computer
	ui->browseSourceButton->setVisible(m_controller->collections().size() == 1);
	connect(ui->browseSourceButton, &QPushButton::clicked, this, &FileCollectDialog::browseSource);

	connect(ui->sourceDirectoryComboBox, &QComboBox::currentTextChanged, this, [this]() {
		handleSettingsChanged(m_settingSourceFromBrowser == false);
	});
	connect(ui->filePatternComboBox, &QComboBox::currentTextChanged, this, [this]() { handleSettingsChanged(false); });
	connect(ui->subfolderHandlingComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
			this, [this]() { handleSettingsChanged(false); });
	connect(m_controller, &FileCollectController::previewChanged, this, [this]() {
		updateSelectionInfo();
		if (m_controller->hasPreview() && m_controller->collections().size() <= 3)
		{
			ui->collectionsTreeView->expandAll();
		}
	});
	updateSelectionInfo();

	connect (ui->browseDestinationDirectoryButton, &QAbstractButton::clicked, this, [this]() {
		const auto dir = QFileDialog::getExistingDirectory(this, tr("Select destination directory"),
														   ui->destinationDirectoryEdit->text());
		if (dir.isEmpty() == false)
		{
			ui->destinationDirectoryEdit->setText(dir);
			m_controller->setDestinationDirectory(dir);
		}
	});

	connect (m_controller, &FileCollectController::overallProgressChanged, this, [this]() {
		ui->progressBar->setValue(m_controller->overallProgress());
	});

	connect (m_controller, &FileCollectController::started, this, [this]() {
		// disable settings during transfer
		ui->settingsGroupBox->setEnabled(false);
		m_previewButton->hide();

		ui->buttonBox->setStandardButtons(QDialogButtonBox::Cancel);
	});

	connect (m_controller, &FileCollectController::finished, this, [this]() {
		ui->buttonBox->setStandardButtons(QDialogButtonBox::Open | QDialogButtonBox::Close);

		const auto darkSuffix = VeyonCore::useDarkMode() ? QStringLiteral("-dark") : QString();
		ui->buttonBox->button(QDialogButtonBox::Open)->setText(tr("Open output directory"));
		ui->buttonBox->button(QDialogButtonBox::Open)->setIcon(QIcon(QStringLiteral(":/core/document-open%1.png").arg(darkSuffix)));
	});

	const auto availableSize = screen()->availableSize();
	move(availableSize.width() / 8, availableSize.height() / 8);
	resize(availableSize * 3 / 4);
}



FileCollectDialog::~FileCollectDialog()
{
	delete ui;
}



void FileCollectDialog::start()
{
	// ITLabControl: nothing to do if every file was unticked in the preview
	if (m_controller->hasPreview())
	{
		bool allPreviewed = true;
		int checkedFiles = 0;
		for (const auto& collection : m_controller->collections())
		{
			allPreviewed &= collection.state == FileCollection::State::Previewed;
			checkedFiles += collection.checkedFilesCount();
		}
		if (allPreviewed && checkedFiles == 0)
		{
			QMessageBox::information(this, tr("No files selected"),
									 tr("No file is ticked in the preview. Tick the files to collect or change the settings."));
			return;
		}
	}

	switch (m_controller->collectionDirectory())
	{
	case FileCollectController::CollectionDirectory::PromptUserForName:
	{
		const auto collectionName = QInputDialog::getText(this, tr("Enter collection name" ),
														  tr("Please enter a name for this file collection:"));
		if (collectionName.isEmpty())
		{
			return;
		}

		m_controller->setCollectionName(collectionName);
		break;
	}
	case FileCollectController::CollectionDirectory::DateTimeSubdirectory:
		m_controller->setCollectionName(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")));
		break;
	default:
		break;
	}

	applySettings();

	if (VeyonCore::filesystem().ensurePathExists(m_controller->outputDirectory()) == false)
	{
		QMessageBox::critical(this, tr("Output directory creation failed"),
							  tr("The output directory \"%1\" does not exist and could not be created. "
								 "Please check the configuration and the file permissions for the configured destination directory.")
							  .arg(m_controller->outputDirectory()));
		return;
	}

	m_controller->start();
}



void FileCollectDialog::applySettings()
{
	// Pass source directory and file pattern to controller
	const auto sourceDir = comboBoxValue(ui->sourceDirectoryComboBox);
	if (sourceDir.isEmpty() == false)
	{
		m_controller->setCollectSourceDirectory(sourceDir);
	}

	// an empty pattern ("All files") must also be passed so a previously used pattern is reset
	m_controller->setFilePattern(comboBoxValue(ui->filePatternComboBox));

	switch (ui->subfolderHandlingComboBox->currentIndex())
	{
	case 1: m_controller->setSubfolderHandling(FileCollectController::SubfolderHandling::NonRecursive); break;
	case 2: m_controller->setSubfolderHandling(FileCollectController::SubfolderHandling::Recursive); break;
	default: m_controller->setSubfolderHandling(FileCollectController::SubfolderHandling::Default); break;
	}
}



void FileCollectDialog::preview()
{
	applySettings();
	m_controller->preview();
}



void FileCollectDialog::browseSource()
{
	if (m_controller->collections().size() != 1)
	{
		return;
	}

	// start in the currently chosen folder if it is a real path, otherwise on the start page
	auto initialPath = comboBoxValue(ui->sourceDirectoryComboBox);
	if (initialPath.contains(QLatin1Char('%')) || QDir::isAbsolutePath(QDir::fromNativeSeparators(initialPath)) == false)
	{
		initialPath.clear();
	}

	RemoteDirectoryDialog dialog(m_controller, m_controller->collections().firstKey(), initialPath, this);
	if (dialog.exec() != QDialog::Accepted)
	{
		return;
	}

	m_settingSourceFromBrowser = true;
	ui->sourceDirectoryComboBox->setEditText(dialog.selectedDirectory());
	m_settingSourceFromBrowser = false;

	m_controller->setFileSelection(dialog.selectedFiles());
	updateSelectionInfo();
}



void FileCollectDialog::handleSettingsChanged(bool sourceEditedByUser)
{
	if (m_controller->isRunning())
	{
		return;
	}

	// files chosen in the browser belong to the folder chosen there
	if (sourceEditedByUser && m_controller->fileSelection().isEmpty() == false)
	{
		m_controller->setFileSelection({});
	}

	// the file lists of a preview no longer match the new settings
	if (m_controller->hasPreview())
	{
		m_controller->discardPreview();
	}

	updateSelectionInfo();
}



void FileCollectDialog::updateSelectionInfo()
{
	QStringList lines;

	const auto& selection = m_controller->fileSelection();
	if (selection.isEmpty() == false)
	{
		auto names = selection.mid(0, 5).join(QStringLiteral(", "));
		if (selection.size() > 5)
		{
			names += QStringLiteral(", ...");
		}
		lines.append(tr("Only the %1 chosen files will be collected: %2").arg(selection.size()).arg(names));
	}

	if (m_controller->hasPreview())
	{
		int computers = 0;
		int checkedFiles = 0;
		int allFiles = 0;
		qint64 checkedSize = 0;
		bool waiting = false;
		for (const auto& collection : m_controller->collections())
		{
			if (collection.state == FileCollection::State::Previewed)
			{
				++computers;
				checkedFiles += collection.checkedFilesCount();
				allFiles += collection.files.size();
				checkedSize += collection.checkedFilesSize();
			}
			else
			{
				waiting = true;
			}
		}
		lines.append(tr("Preview: %1 of %2 files ticked (%3) on %4 computers. "
						"Untick the files you do not need, then click \"Start\".")
					 .arg(checkedFiles).arg(allFiles).arg(QLocale().formattedDataSize(checkedSize)).arg(computers));
		if (waiting)
		{
			lines.append(tr("Waiting for the file lists of the other computers..."));
		}
	}
	else if (selection.isEmpty())
	{
		lines.append(tr("Tip: click \"Preview files\" to see and choose the files before collecting them."));
	}

	ui->selectionLabel->setText(lines.join(QLatin1Char('\n')));
}



void FileCollectDialog::openOutputDirectory()
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(m_controller->outputDirectory() + QDir::separator()));

}



void FileCollectDialog::accept()
{
	if (ui->buttonBox->standardButtons().testFlag(QDialogButtonBox::Open))
	{
		openOutputDirectory();
	}
	else
	{
		start();
	}
}



void FileCollectDialog::reject()
{
	if (m_controller->isRunning())
	{
		m_controller->stop();
		ui->buttonBox->setStandardButtons(QDialogButtonBox::Close);
	}
	else
	{
		// ITLabControl: release the file lists kept on the student computers for the preview
		m_controller->discardPreview();
		m_controller->setFileSelection({});
		QDialog::reject();
	}
}
