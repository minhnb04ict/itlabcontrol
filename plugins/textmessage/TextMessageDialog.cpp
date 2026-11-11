/*
 * TextMessageDialog.cpp - implementation of text message dialog class
 *
 * Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QMessageBox>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextDocument>

#include "TextMessageDialog.h"
#include "VeyonCore.h"
#include "PlatformPluginInterface.h"
#include "PlatformUserFunctions.h"

#include "ui_TextMessageDialog.h"


TextMessageDialog::TextMessageDialog( QString &msgStr, QString &titleStr, QWidget *parent ) :
	QDialog( parent ),
	ui( new Ui::TextMessageDialog ),
	m_msgStr( msgStr ),
	m_titleStr( titleStr )
{
	ui->setupUi( this );

	auto username = VeyonCore::platform().userFunctions().queryCurrentUserProperty(PlatformUserFunctions::UserProperty::FullName);
	if (username.isEmpty())
	{
		username = VeyonCore::platform().userFunctions().queryCurrentUserProperty(PlatformUserFunctions::UserProperty::LoginName);
	}

	if (username.isEmpty())
	{
		ui->titleEdit->setText(tr("Message from teacher"));
	}
	else
	{
		ui->titleEdit->setText(tr("Message from %1").arg(username));
	}
}



TextMessageDialog::~TextMessageDialog()
{
	delete ui;
}



// ITLabControl: turn web addresses typed as plain text into clickable links
static void linkifyDocument( QTextDocument* document )
{
	static const QRegularExpression urlRegEx( QStringLiteral(R"(\b(?:(?:https?|ftp)://|www\.)[^\s<>"]+)"),
											   QRegularExpression::CaseInsensitiveOption );
	static const QString trailingPunctuation = QStringLiteral(".,;:!?)]}'");

	QTextCursor cursor( document );
	while( true )
	{
		cursor = document->find( urlRegEx, cursor );
		if( cursor.isNull() )
		{
			break;
		}

		const int start = cursor.selectionStart();
		int end = cursor.selectionEnd();
		QString url = cursor.selectedText();
		while( url.isEmpty() == false && trailingPunctuation.contains( url.back() ) )
		{
			url.chop( 1 );
			--end;
		}

		cursor.setPosition( end );
		if( url.isEmpty() )
		{
			continue;
		}

		QTextCursor linkCursor( document );
		linkCursor.setPosition( start );
		linkCursor.setPosition( end, QTextCursor::KeepAnchor );
		if( linkCursor.charFormat().isAnchor() )
		{
			continue; // already a link (e.g. pasted from a web page)
		}

		QTextCharFormat linkFormat;
		linkFormat.setAnchor( true );
		linkFormat.setAnchorHref( url.startsWith( QLatin1String("www."), Qt::CaseInsensitive ) ?
									  QStringLiteral("https://") + url : url );
		linkFormat.setForeground( QColor( 0, 102, 204 ) );
		linkFormat.setFontUnderline( true );
		linkCursor.mergeCharFormat( linkFormat );
	}
}



void TextMessageDialog::accept()
{
	linkifyDocument( ui->textEdit->document() );
	m_msgStr = ui->textEdit->toPlainText().trimmed().isEmpty() ? QString{} : ui->textEdit->toHtml();
	m_titleStr = ui->titleEdit->text().trimmed();
	QDialog::accept();
}
