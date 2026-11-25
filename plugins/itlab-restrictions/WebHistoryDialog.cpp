/*
 * WebHistoryDialog.cpp - show and export the web browsing history of student computers (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <QDateEdit>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "WebHistory.h"
#include "WebHistoryDialog.h"


namespace {

enum Column { ComputerColumn, TimeColumn, UserColumn, BrowserColumn, TitleColumn, UrlColumn, ColumnCount };

// sorts the time column by the timestamp, not by the displayed text
class HistoryItem : public QTreeWidgetItem
{
public:
	using QTreeWidgetItem::QTreeWidgetItem;

	bool operator<( const QTreeWidgetItem& other ) const override
	{
		const auto column = treeWidget() ? treeWidget()->sortColumn() : 0;
		if( column == TimeColumn )
		{
			return data( TimeColumn, Qt::UserRole ).toLongLong() < other.data( TimeColumn, Qt::UserRole ).toLongLong();
		}
		return QTreeWidgetItem::operator<( other );
	}
};

QString csvField( const QString& text )
{
	auto escaped = text;
	escaped.replace( QLatin1Char('"'), QStringLiteral("\"\"") );
	return QLatin1Char('"') + escaped + QLatin1Char('"');
}

}



WebHistoryDialog::WebHistoryDialog( const ComputerControlInterfaceList& computers, const RequestFunction& request,
									QWidget* parent ) :
	QDialog( parent ),
	m_computers( computers ),
	m_request( request ),
	m_fromEdit( new QDateEdit( QDate::currentDate(), this ) ),
	m_toEdit( new QDateEdit( QDate::currentDate(), this ) ),
	m_filterEdit( new QLineEdit( this ) ),
	m_table( new QTreeWidget( this ) ),
	m_statusLabel( new QLabel( this ) )
{
	setWindowTitle( tr( "Web browsing history" ) );
	resize( 1100, 640 );
	setAttribute( Qt::WA_DeleteOnClose );

	const auto oldest = QDate::currentDate().addDays( -WebHistory::retentionDays() );
	for( auto edit : { m_fromEdit, m_toEdit } )
	{
		edit->setCalendarPopup( true );
		edit->setMinimumDate( oldest );
		edit->setMaximumDate( QDate::currentDate() );
		edit->setDisplayFormat( QStringLiteral("dd/MM/yyyy") );
	}

	auto loadButton = new QPushButton( tr( "Load" ), this );
	m_filterEdit->setPlaceholderText( tr( "Search in computer, user, title and address" ) );
	m_filterEdit->setClearButtonEnabled( true );

	auto topLayout = new QHBoxLayout;
	topLayout->addWidget( new QLabel( tr( "From:" ), this ) );
	topLayout->addWidget( m_fromEdit );
	topLayout->addWidget( new QLabel( tr( "to:" ), this ) );
	topLayout->addWidget( m_toEdit );
	topLayout->addWidget( loadButton );
	topLayout->addSpacing( 20 );
	topLayout->addWidget( m_filterEdit, 1 );

	m_table->setRootIsDecorated( false );
	m_table->setUniformRowHeights( true );
	m_table->setSortingEnabled( true );
	m_table->setHeaderLabels( { tr( "Computer" ), tr( "Time" ), tr( "User" ), tr( "Browser" ), tr( "Title" ), tr( "Address" ) } );
	m_table->header()->setSectionResizeMode( TitleColumn, QHeaderView::Interactive );
	m_table->setColumnWidth( ComputerColumn, 130 );
	m_table->setColumnWidth( TimeColumn, 140 );
	m_table->setColumnWidth( UserColumn, 110 );
	m_table->setColumnWidth( BrowserColumn, 80 );
	m_table->setColumnWidth( TitleColumn, 300 );

	auto hint = new QLabel( tr( "The history of the last %1 days is kept on each student computer. Pages opened in "
								"private/incognito windows are not recorded by the browsers." ).arg( WebHistory::retentionDays() ), this );
	hint->setWordWrap( true );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Close, this );
	auto exportButton = buttons->addButton( tr( "Export (CSV for Excel)..." ), QDialogButtonBox::ActionRole );

	auto layout = new QVBoxLayout( this );
	layout->addLayout( topLayout );
	layout->addWidget( m_table, 1 );
	layout->addWidget( m_statusLabel );
	layout->addWidget( hint );
	layout->addWidget( buttons );

	connect( loadButton, &QPushButton::clicked, this, &WebHistoryDialog::load );
	connect( m_filterEdit, &QLineEdit::textChanged, this, &WebHistoryDialog::applyFilter );
	connect( exportButton, &QPushButton::clicked, this, &WebHistoryDialog::exportCsv );
	connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

	load();
}



void WebHistoryDialog::load()
{
	m_table->clear();
	m_answered = 0;
	m_requested = 0;

	ComputerControlInterfaceList connected;
	for( const auto& computer : std::as_const( m_computers ) )
	{
		if( computer->state() == ComputerControlInterface::State::Connected )
		{
			connected.append( computer );
		}
	}
	m_requested = connected.size();

	const auto fromMs = QDateTime( m_fromEdit->date(), QTime( 0, 0 ) ).toMSecsSinceEpoch();
	const auto toMs = QDateTime( m_toEdit->date(), QTime( 23, 59, 59, 999 ) ).toMSecsSinceEpoch();
	if( connected.isEmpty() == false )
	{
		m_request( connected, fromMs, toMs );
	}
	updateStatus();
}



void WebHistoryDialog::addEntries( ComputerControlInterface::Pointer computer, const QVariantList& entries )
{
	if( m_computers.contains( computer ) == false )
	{
		return;
	}

	m_table->setSortingEnabled( false );
	const auto computerName = computer->computer().displayName();
	for( const auto& entryValue : entries )
	{
		const auto entry = entryValue.toList();
		const auto ms = entry.value( 0 ).toLongLong();
		auto item = new HistoryItem( m_table );
		item->setText( ComputerColumn, computerName );
		item->setText( TimeColumn, QDateTime::fromMSecsSinceEpoch( ms ).toString( QStringLiteral("dd/MM/yyyy HH:mm:ss") ) );
		item->setData( TimeColumn, Qt::UserRole, ms );
		item->setText( UserColumn, entry.value( 1 ).toString() );
		item->setText( BrowserColumn, entry.value( 2 ).toString() );
		item->setText( UrlColumn, entry.value( 3 ).toString() );
		item->setText( TitleColumn, entry.value( 4 ).toString() );
		item->setToolTip( UrlColumn, entry.value( 3 ).toString() );
	}
	m_table->setSortingEnabled( true );
	m_table->sortItems( TimeColumn, Qt::DescendingOrder );

	++m_answered;
	applyFilter();
	updateStatus();
}



void WebHistoryDialog::applyFilter()
{
	const auto filter = m_filterEdit->text().trimmed();
	for( int i = 0; i < m_table->topLevelItemCount(); ++i )
	{
		auto item = m_table->topLevelItem( i );
		bool visible = filter.isEmpty();
		for( int column = 0; visible == false && column < ColumnCount; ++column )
		{
			visible = item->text( column ).contains( filter, Qt::CaseInsensitive );
		}
		item->setHidden( visible == false );
	}
}



void WebHistoryDialog::updateStatus()
{
	if( m_requested == 0 )
	{
		m_statusLabel->setText( tr( "None of the computers is connected." ) );
		return;
	}
	m_statusLabel->setText( tr( "%1 of %2 computers answered, %3 entries." )
							.arg( m_answered ).arg( m_requested ).arg( m_table->topLevelItemCount() ) );
}



void WebHistoryDialog::exportCsv()
{
	const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export web browsing history" ),
		QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) +
			QStringLiteral("/LichSuWeb_%1.csv").arg( QDate::currentDate().toString( QStringLiteral("yyyy-MM-dd") ) ),
		tr( "CSV files (*.csv)" ) );
	if( fileName.isEmpty() )
	{
		return;
	}

	QFile file( fileName );
	if( file.open( QFile::WriteOnly ) == false )
	{
		QMessageBox::critical( this, tr( "Export web browsing history" ), tr( "The file could not be written." ) );
		return;
	}

	// UTF-8 with BOM so that Excel shows Vietnamese characters correctly
	file.write( "\xEF\xBB\xBF" );
	QStringList header;
	for( int column = 0; column < ColumnCount; ++column )
	{
		header.append( csvField( m_table->headerItem()->text( column ) ) );
	}
	file.write( header.join( QLatin1Char(',') ).toUtf8() + "\r\n" );

	int count = 0;
	for( int i = 0; i < m_table->topLevelItemCount(); ++i )
	{
		const auto item = m_table->topLevelItem( i );
		if( item->isHidden() )
		{
			continue;
		}
		QStringList fields;
		for( int column = 0; column < ColumnCount; ++column )
		{
			fields.append( csvField( item->text( column ) ) );
		}
		file.write( fields.join( QLatin1Char(',') ).toUtf8() + "\r\n" );
		++count;
	}

	QMessageBox::information( this, tr( "Export web browsing history" ), tr( "%1 entries exported." ).arg( count ) );
}
