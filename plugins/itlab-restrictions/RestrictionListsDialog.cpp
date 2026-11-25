/*
 * RestrictionListsDialog.cpp - view and edit the website and application lists (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "AppListDialog.h"
#include "Configuration/Object.h"
#include "RestrictionListsDialog.h"
#include "RestrictionsBackend.h"


QString RestrictionListsDialog::configGroup() { return QStringLiteral("ITLabRestrictions"); }
QString RestrictionListsDialog::allowedWebsitesKey() { return QStringLiteral("AllowedWebsites"); }
QString RestrictionListsDialog::blockedWebsitesKey() { return QStringLiteral("BlockedWebsites"); }
QString RestrictionListsDialog::allowedApplicationsKey() { return QStringLiteral("AllowedApplications"); }
QString RestrictionListsDialog::blockedApplicationsKey() { return QStringLiteral("BlockedApplications"); }



RestrictionListsDialog::RestrictionListsDialog( Configuration::Object* config, QWidget* parent ) :
	QDialog( parent ),
	m_config( config )
{
	setWindowTitle( tr( "Website and application lists" ) );
	resize( 640, 520 );

	auto description = new QLabel( tr( "Prepare the lists here without restricting anything. They are used when you choose "
										"a restriction in the \"Restrictions\" menu." ), this );
	description->setWordWrap( true );

	// the lambdas below refer to tabs by index, so the list must not grow later
	m_tabs = {
		{ allowedWebsitesKey(), true, true },
		{ blockedWebsitesKey(), true, false },
		{ allowedApplicationsKey(), false, true },
		{ blockedApplicationsKey(), false, false },
	};

	auto tabWidget = new QTabWidget( this );
	tabWidget->addTab( createTab( m_tabs[0] ), tr( "Allowed websites" ) );
	tabWidget->addTab( createTab( m_tabs[1] ), tr( "Blocked websites" ) );
	tabWidget->addTab( createTab( m_tabs[2] ), tr( "Allowed applications" ) );
	tabWidget->addTab( createTab( m_tabs[3] ), tr( "Blocked applications" ) );

	auto buttonBox = new QDialogButtonBox( QDialogButtonBox::Save | QDialogButtonBox::Close, this );
	connect( buttonBox->button( QDialogButtonBox::Save ), &QPushButton::clicked, this, &RestrictionListsDialog::accept );
	connect( buttonBox->button( QDialogButtonBox::Close ), &QPushButton::clicked, this, &RestrictionListsDialog::reject );

	auto layout = new QVBoxLayout( this );
	layout->addWidget( description );
	layout->addWidget( tabWidget, 1 );
	layout->addWidget( buttonBox );

	for( auto& tab : m_tabs )
	{
		setEntries( tab, normalized( tab, m_config->value( tab.configKey, configGroup(), QStringList{} ).toStringList() ) );
	}
	m_modified = false;
}



void RestrictionListsDialog::accept()
{
	save();
	QDialog::accept();
}



void RestrictionListsDialog::reject()
{
	if( m_modified )
	{
		const auto answer = QMessageBox::question( this, windowTitle(), tr( "The lists have been changed. Save the changes?" ),
												   QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel );
		if( answer == QMessageBox::Cancel )
		{
			return;
		}
		if( answer == QMessageBox::Save )
		{
			save();
		}
	}

	QDialog::reject();
}



QWidget* RestrictionListsDialog::createTab( ListTab& tab )
{
	const auto index = int( &tab - m_tabs.data() );

	auto widget = new QWidget;

	QString hintText;
	if( tab.websites )
	{
		hintText = tab.allowList ? tr( "Websites which can be opened with \"Allow only listed websites\". Subdomains are included." )
								 : tr( "Websites which are blocked with \"Block listed websites\". Subdomains are included." );
	}
	else
	{
		hintText = tab.allowList ? tr( "Programs which can be used with \"Allow only listed applications\" "
									   "(programs of Windows always stay allowed)." )
								 : tr( "Programs which are closed with \"Block listed applications\". "
									   "Enter the program file name, e.g. zalo.exe (see Task Manager, tab \"Details\")." );
	}
	auto hint = new QLabel( hintText, widget );
	hint->setWordWrap( true );

	tab.list = new QListWidget( widget );
	tab.list->setSelectionMode( QAbstractItemView::ExtendedSelection );
	tab.list->setSortingEnabled( true );
	tab.countLabel = new QLabel( widget );

	auto addButton = new QPushButton( tr( "Add..." ), widget );
	auto editButton = new QPushButton( tr( "Edit..." ), widget );
	auto removeButton = new QPushButton( tr( "Remove" ), widget );
	connect( addButton, &QPushButton::clicked, this, [this, index]() { addEntries( m_tabs[index] ); } );
	connect( editButton, &QPushButton::clicked, this, [this, index]() { editEntry( m_tabs[index] ); } );
	connect( tab.list, &QListWidget::itemDoubleClicked, this, [this, index]() {
		// the item is replaced while editing - not inside the double-click handling of the list
		QMetaObject::invokeMethod( this, [this, index]() { editEntry( m_tabs[index] ); }, Qt::QueuedConnection );
	} );
	connect( removeButton, &QPushButton::clicked, this, [this, index]() { removeEntries( m_tabs[index] ); } );

	auto buttons = new QVBoxLayout;
	buttons->addWidget( addButton );
	buttons->addWidget( editButton );
	buttons->addWidget( removeButton );

	if( tab.websites == false )
	{
		auto presetButton = new QToolButton( widget );
		presetButton->setText( tr( "Add group" ) );
		presetButton->setPopupMode( QToolButton::InstantPopup );
		presetButton->setToolButtonStyle( Qt::ToolButtonTextOnly );
		auto menu = new QMenu( presetButton );
		for( const auto& preset : AppListDialog::presets( tab.allowList ) )
		{
			const auto names = preset.second;
			menu->addAction( preset.first, this, [this, index, names]() { addPreset( m_tabs[index], names ); } );
		}
		presetButton->setMenu( menu );
		buttons->addWidget( presetButton );
	}
	buttons->addStretch( 1 );

	auto listLayout = new QHBoxLayout;
	listLayout->addWidget( tab.list, 1 );
	listLayout->addLayout( buttons );

	auto layout = new QVBoxLayout( widget );
	layout->addWidget( hint );
	layout->addLayout( listLayout, 1 );
	layout->addWidget( tab.countLabel );

	return widget;
}



QStringList RestrictionListsDialog::normalized( const ListTab& tab, const QStringList& entries ) const
{
	return tab.websites ? RestrictionsBackend::normalizedWebsites( entries ) : RestrictionsBackend::normalizedApps( entries );
}



bool RestrictionListsDialog::askWebsites( const QString& title, const QString& initialEntry, bool multiLine,
										  QStringList* result )
{
	QDialog dialog( this );
	dialog.setWindowTitle( title );
	dialog.resize( 560, multiLine ? 420 : 260 );

	const auto initialExact = initialEntry.startsWith( QLatin1Char('=') );
	const auto initialText = initialExact ? initialEntry.mid( 1 ) : initialEntry;

	QPlainTextEdit* textEdit = nullptr;
	QLineEdit* lineEdit = nullptr;
	if( multiLine )
	{
		textEdit = new QPlainTextEdit( &dialog );
		textEdit->setPlaceholderText( QStringLiteral("chat.google.com\nfacebook.com\nyoutube.com/shorts") );
	}
	else
	{
		lineEdit = new QLineEdit( initialText, &dialog );
	}

	auto typeComboBox = new QComboBox( &dialog );
	typeComboBox->addItem( tr( "Whole domain (including sub domains)" ) );
	typeComboBox->addItem( tr( "Exactly this address (without sub domains)" ) );
	typeComboBox->setCurrentIndex( initialExact ? 1 : 0 );

	auto hint = new QLabel( tr( "Examples:<br>"
								"<b>google.com</b> - google.com and all sub domains (mail.google.com, chat.google.com, ...)<br>"
								"<b>chat.google.com</b> - only chat.google.com, the search on google.com still works<br>"
								"<b>youtube.com/shorts</b> - only the Shorts pages of YouTube<br>"
								"With <i>Exactly this address</i>, <b>www.google.com</b> only matches www.google.com, "
								"not google.com or other sub domains.<br>"
								"Note: Google Chat is also available inside Gmail at <b>mail.google.com/chat</b>." ), &dialog );
	hint->setWordWrap( true );

	auto form = new QFormLayout;
	form->addRow( tr( "Type:" ), typeComboBox );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog );
	connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
	connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );

	auto layout = new QVBoxLayout( &dialog );
	layout->addWidget( new QLabel( multiLine ? tr( "Websites, one per line:" ) : tr( "Website:" ), &dialog ) );
	if( textEdit )
	{
		layout->addWidget( textEdit, 1 );
	}
	else
	{
		layout->addWidget( lineEdit );
	}
	layout->addLayout( form );
	layout->addWidget( hint );
	layout->addWidget( buttons );

	if( dialog.exec() != QDialog::Accepted )
	{
		return false;
	}

	const auto lines = textEdit ? textEdit->toPlainText().split( QLatin1Char('\n') ) : QStringList{ lineEdit->text() };
	const auto exact = typeComboBox->currentIndex() == 1;
	result->clear();
	for( auto line : lines )
	{
		line = line.trimmed();
		while( line.startsWith( QLatin1Char('=') ) )
		{
			line = line.mid( 1 ).trimmed();
		}
		if( line.isEmpty() == false )
		{
			result->append( exact ? QStringLiteral("=") + line : line );
		}
	}
	return true;
}



QString RestrictionListsDialog::displayText( const ListTab& tab, const QString& entry )
{
	if( tab.websites == false )
	{
		return entry;
	}
	if( entry.startsWith( QLatin1Char('=') ) )
	{
		return entry.mid( 1 ) + QStringLiteral("    ") + tr( "(exactly this address)" );
	}
	return entry + QStringLiteral("    ") + tr( "(including sub domains)" );
}



void RestrictionListsDialog::addEntries( ListTab& tab )
{
	QStringList lines;
	if( tab.websites )
	{
		if( askWebsites( tr( "Add" ), {}, true, &lines ) == false )
		{
			return;
		}
	}
	else
	{
		bool ok = false;
		const auto text = QInputDialog::getMultiLineText( this, tr( "Add" ),
														  tr( "Program file names, one per line (e.g. zalo.exe):" ), {}, &ok );
		if( ok == false )
		{
			return;
		}
		lines = text.split( QLatin1Char('\n') );
	}

	const auto newEntries = normalized( tab, lines );
	if( newEntries.isEmpty() )
	{
		QMessageBox::information( this, tr( "Add" ), tr( "No valid entry was entered." ) );
		return;
	}

	addPreset( tab, newEntries );
}



void RestrictionListsDialog::editEntry( ListTab& tab )
{
	const auto item = tab.list->currentItem();
	if( item == nullptr )
	{
		QMessageBox::information( this, tr( "Edit" ), tr( "Please select an entry first." ) );
		return;
	}

	const auto oldEntry = item->data( Qt::UserRole ).toString();
	QStringList lines;
	if( tab.websites )
	{
		if( askWebsites( tr( "Edit" ), oldEntry, false, &lines ) == false )
		{
			return;
		}
	}
	else
	{
		bool ok = false;
		const auto text = QInputDialog::getText( this, tr( "Edit" ), tr( "Program file name:" ),
												 QLineEdit::Normal, oldEntry, &ok );
		if( ok == false )
		{
			return;
		}
		lines = QStringList{ text };
	}

	const auto newEntry = normalized( tab, lines ).value( 0 );
	if( newEntry.isEmpty() )
	{
		QMessageBox::information( this, tr( "Edit" ), tr( "The entry is not valid." ) );
		return;
	}
	if( newEntry == oldEntry )
	{
		return;
	}

	auto current = entries( tab );
	if( current.contains( newEntry ) )
	{
		QMessageBox::information( this, tr( "Edit" ), tr( "This entry is already in the list." ) );
		return;
	}
	current.replace( current.indexOf( oldEntry ), newEntry );
	setEntries( tab, current );
}



void RestrictionListsDialog::removeEntries( ListTab& tab )
{
	const auto selected = tab.list->selectedItems();
	if( selected.isEmpty() )
	{
		QMessageBox::information( this, tr( "Remove" ), tr( "Please select one or more entries first." ) );
		return;
	}

	if( QMessageBox::question( this, tr( "Remove" ), tr( "Remove %1 selected entries?" ).arg( selected.size() ) ) != QMessageBox::Yes )
	{
		return;
	}

	QStringList removed;
	for( const auto item : selected )
	{
		removed.append( item->data( Qt::UserRole ).toString() );
	}

	auto current = entries( tab );
	for( const auto& entry : std::as_const( removed ) )
	{
		current.removeAll( entry );
	}
	setEntries( tab, current );
}



void RestrictionListsDialog::addPreset( ListTab& tab, const QStringList& newEntries )
{
	auto current = entries( tab );
	for( const auto& entry : newEntries )
	{
		if( current.contains( entry ) == false )
		{
			current.append( entry );
		}
	}
	setEntries( tab, current );
}



void RestrictionListsDialog::setEntries( ListTab& tab, const QStringList& newEntries )
{
	tab.list->clear();
	for( const auto& entry : newEntries )
	{
		auto item = new QListWidgetItem( displayText( tab, entry ), tab.list );
		item->setData( Qt::UserRole, entry );
	}
	m_modified = true;
	updateCount( tab );
}



QStringList RestrictionListsDialog::entries( const ListTab& tab ) const
{
	QStringList result;
	for( int i = 0; i < tab.list->count(); ++i )
	{
		result.append( tab.list->item( i )->data( Qt::UserRole ).toString() );
	}
	return result;
}



void RestrictionListsDialog::updateCount( ListTab& tab )
{
	tab.countLabel->setText( tr( "%1 entries" ).arg( tab.list->count() ) );
}



void RestrictionListsDialog::save()
{
	for( const auto& tab : std::as_const( m_tabs ) )
	{
		m_config->setValue( tab.configKey, entries( tab ), configGroup() );
	}
	m_config->flushStore();
	m_modified = false;
}
