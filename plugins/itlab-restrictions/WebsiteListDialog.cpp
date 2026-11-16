/*
 * WebsiteListDialog.cpp - edit the list of allowed or blocked websites (ITLabControl)
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
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "RestrictionsBackend.h"
#include "WebsiteListDialog.h"


bool WebsiteListDialog::edit( QWidget* parent, bool allowList, QStringList& websites )
{
	QDialog dialog( parent );
	dialog.setWindowTitle( allowList ? tr( "Allow only listed websites" ) : tr( "Block listed websites" ) );
	dialog.resize( 520, 440 );

	auto description = new QLabel( allowList ?
		tr( "Only these websites can be opened on the student computers. Enter one website per line, "
			"e.g. <b>vnexpress.net</b> or <b>https://www.google.com</b>. Subdomains are included "
			"automatically (google.com also allows mail.google.com).<br><br>"
			"Note: many websites load content from other websites (images, videos, sign-in). "
			"If a page is not displayed correctly, add these websites as well." )
		:
		tr( "These websites can not be opened on the student computers, all others can. "
			"Enter one website per line, e.g. <b>facebook.com</b> or <b>https://www.youtube.com</b>. "
			"Subdomains are included automatically." ), &dialog );
	description->setWordWrap( true );

	auto edit = new QPlainTextEdit( &dialog );
	edit->setPlainText( websites.join( QLatin1Char('\n') ) );
	edit->setPlaceholderText( allowList ? QStringLiteral("vnexpress.net\nwikipedia.org\nedu.vn")
										: QStringLiteral("facebook.com\nyoutube.com\ntiktok.com") );

	auto hint = new QLabel( &dialog );
	hint->setWordWrap( true );

	auto buttonBox = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog );
	buttonBox->button( QDialogButtonBox::Ok )->setText( allowList ? tr( "Allow only these websites" )
																  : tr( "Block these websites" ) );

	const auto updateHint = [=]() {
		const auto hosts = RestrictionsBackend::normalizedWebsites( edit->toPlainText().split( QLatin1Char('\n') ) );
		hint->setText( tr( "%1 websites: %2" ).arg( hosts.size() ).arg( hosts.mid( 0, 8 ).join( QStringLiteral(", ") ) +
																		 ( hosts.size() > 8 ? QStringLiteral(", ...") : QString{} ) ) );
	};
	QObject::connect( edit, &QPlainTextEdit::textChanged, &dialog, updateHint );
	updateHint();

	QObject::connect( buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
	QObject::connect( buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );

	auto layout = new QVBoxLayout( &dialog );
	layout->addWidget( description );
	layout->addWidget( edit );
	layout->addWidget( hint );
	layout->addWidget( buttonBox );

	if( dialog.exec() != QDialog::Accepted )
	{
		return false;
	}

	websites = edit->toPlainText().split( QLatin1Char('\n') );
	for( auto& website : websites )
	{
		website = website.trimmed();
	}
	websites.removeAll( QString{} );

	if( allowList && RestrictionsBackend::normalizedWebsites( websites ).isEmpty() &&
		QMessageBox::question( parent, dialog.windowTitle(),
							   tr( "The list is empty, so no website can be opened at all. Continue?" ) ) != QMessageBox::Yes )
	{
		return false;
	}

	return true;
}
