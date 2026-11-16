/*
 * AppListDialog.cpp - edit the list of allowed or blocked applications (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "AppListDialog.h"
#include "RestrictionsBackend.h"


bool AppListDialog::edit( QWidget* parent, bool allowList, QStringList& apps )
{
	QDialog dialog( parent );
	dialog.setWindowTitle( allowList ? tr( "Allow only listed applications" ) : tr( "Block listed applications" ) );
	dialog.resize( 560, 480 );

	auto description = new QLabel( allowList ?
		tr( "Only these applications can be used on the student computers, other applications are closed "
			"as soon as they open a window. Programs of Windows (Explorer, Notepad, Paint, Calculator, ...) "
			"and Vietnamese input methods (UniKey, EVKey) always stay allowed.<br><br>"
			"Enter one program file name per line, e.g. <b>winword.exe</b>." )
		:
		tr( "These applications are closed immediately on the student computers whenever they are started. "
			"Enter one program file name per line, e.g. <b>zalo.exe</b>. "
			"Tip: the file name is shown in the Task Manager (tab \"Details\")." ), &dialog );
	description->setWordWrap( true );

	auto edit = new QPlainTextEdit( &dialog );
	edit->setPlainText( apps.join( QLatin1Char('\n') ) );

	// quick add buttons for common groups of programs
	const QList<QPair<QString, QStringList>> presets = allowList ?
		QList<QPair<QString, QStringList>>{
			{ tr( "Office" ), { QStringLiteral("winword.exe"), QStringLiteral("excel.exe"), QStringLiteral("powerpnt.exe"),
								 QStringLiteral("onenote.exe") } },
			{ tr( "Programming" ), { QStringLiteral("code.exe"), QStringLiteral("devcpp.exe"), QStringLiteral("codeblocks.exe"),
									  QStringLiteral("python.exe"), QStringLiteral("pythonw.exe"), QStringLiteral("scratch 3.exe") } },
			{ tr( "Web browsers" ), RestrictionsBackend::browserProcessNames() },
		}
		:
		QList<QPair<QString, QStringList>>{
			{ tr( "Web browsers" ), RestrictionsBackend::browserProcessNames() },
			{ tr( "Chat and social media" ), { QStringLiteral("zalo.exe"), QStringLiteral("telegram.exe"), QStringLiteral("discord.exe"),
												QStringLiteral("messenger.exe"), QStringLiteral("skype.exe") } },
			{ tr( "Games" ), { QStringLiteral("steam.exe"), QStringLiteral("epicgameslauncher.exe"), QStringLiteral("riotclientservices.exe"),
								QStringLiteral("leagueclient.exe"), QStringLiteral("robloxplayerbeta.exe"), QStringLiteral("garena.exe") } },
		};

	auto presetLayout = new QHBoxLayout;
	presetLayout->addWidget( new QLabel( tr( "Add:" ), &dialog ) );
	for( const auto& preset : presets )
	{
		auto button = new QPushButton( QStringLiteral("+ ") + preset.first, &dialog );
		const auto names = preset.second;
		QObject::connect( button, &QPushButton::clicked, &dialog, [edit, names]() {
			auto current = RestrictionsBackend::normalizedApps( edit->toPlainText().split( QLatin1Char('\n') ) );
			for( const auto& name : names )
			{
				if( current.contains( name ) == false )
				{
					current.append( name );
				}
			}
			edit->setPlainText( current.join( QLatin1Char('\n') ) );
		} );
		presetLayout->addWidget( button );
	}
	presetLayout->addStretch( 1 );

	auto hint = new QLabel( &dialog );
	hint->setWordWrap( true );
	const auto updateHint = [=]() {
		const auto names = RestrictionsBackend::normalizedApps( edit->toPlainText().split( QLatin1Char('\n') ) );
		hint->setText( tr( "%1 applications: %2" ).arg( names.size() ).arg( names.mid( 0, 8 ).join( QStringLiteral(", ") ) +
																		   ( names.size() > 8 ? QStringLiteral(", ...") : QString{} ) ) );
	};
	QObject::connect( edit, &QPlainTextEdit::textChanged, &dialog, updateHint );
	updateHint();

	auto buttonBox = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog );
	buttonBox->button( QDialogButtonBox::Ok )->setText( allowList ? tr( "Allow only these applications" )
																  : tr( "Block these applications" ) );
	QObject::connect( buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
	QObject::connect( buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );

	auto layout = new QVBoxLayout( &dialog );
	layout->addWidget( description );
	layout->addLayout( presetLayout );
	layout->addWidget( edit );
	layout->addWidget( hint );
	layout->addWidget( buttonBox );

	if( dialog.exec() != QDialog::Accepted )
	{
		return false;
	}

	apps = RestrictionsBackend::normalizedApps( edit->toPlainText().split( QLatin1Char('\n') ) );

	if( allowList && apps.isEmpty() &&
		QMessageBox::question( parent, dialog.windowTitle(),
							   tr( "The list is empty, so only programs of Windows can be used. Continue?" ) ) != QMessageBox::Yes )
	{
		return false;
	}

	return true;
}
