/*
 * SurveyResultsDialog.cpp - live results of a quick survey in the Teacher Console (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <algorithm>

#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include "SurveyResultsDialog.h"


namespace {

using Type = SurveyQuestion::Type;

constexpr int MaxChartRows = 12;
constexpr int RowHeight = 32;

const QColor DefaultColor( 0x21, 0x96, 0xf3 );
const QColor CorrectColor( 0x4c, 0xaf, 0x50 );
const QColor TrafficLightColors[] = { QColor( 0x4c, 0xaf, 0x50 ), QColor( 0xff, 0xc1, 0x07 ), QColor( 0xf4, 0x43, 0x36 ) };

QString csvField( const QString& text )
{
	auto escaped = text;
	escaped.replace( QLatin1Char('"'), QStringLiteral("\"\"") );
	return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString csvLine( const QStringList& fields )
{
	QStringList escaped;
	for( const auto& field : fields )
	{
		escaped.append( csvField( field ) );
	}
	return escaped.join( QLatin1Char(',') ) + QStringLiteral("\r\n");
}

QString percent( int count, int total )
{
	return total > 0 ? QStringLiteral("%1%").arg( qRound( 100.0 * count / total ) ) : QStringLiteral("0%");
}

QString duration( qint64 seconds )
{
	return QStringLiteral("%1:%2").arg( seconds / 60, 2, 10, QLatin1Char('0') ).arg( seconds % 60, 2, 10, QLatin1Char('0') );
}

}



// ═════════════════════════════════════════════════════════════════════════════
//  bar chart (drawn with QPainter, Qt Charts is GPLv3 only)
// ═════════════════════════════════════════════════════════════════════════════

SurveyChartWidget::SurveyChartWidget( QWidget* parent ) :
	QWidget( parent )
{
	setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Minimum );
}



void SurveyChartWidget::setRows( const QList<Row>& rows, int total )
{
	m_rows = rows;
	m_total = total;
	updateGeometry();
	setMinimumHeight( sizeHint().height() );
	update();
}



QSize SurveyChartWidget::sizeHint() const
{
	return { 500, qMax( 1, int( m_rows.size() ) ) * RowHeight + 8 };
}



void SurveyChartWidget::paintEvent( QPaintEvent* event )
{
	Q_UNUSED(event)

	QPainter painter( this );
	painter.setRenderHint( QPainter::Antialiasing );

	if( m_rows.isEmpty() )
	{
		painter.setPen( palette().color( QPalette::PlaceholderText ) );
		painter.drawText( rect(), Qt::AlignCenter, tr( "No answers yet" ) );
		return;
	}

	const auto metrics = painter.fontMetrics();
	int labelWidth = 0;
	for( const auto& row : std::as_const( m_rows ) )
	{
		labelWidth = qMax( labelWidth, metrics.horizontalAdvance( row.label ) );
	}
	labelWidth = qMin( labelWidth + 12, width() * 2 / 5 );
	const auto valueWidth = metrics.horizontalAdvance( QStringLiteral("9999 (100%) ✓") ) + 12;
	const auto barWidth = qMax( 20, width() - labelWidth - valueWidth - 8 );

	int maxCount = 1;
	for( const auto& row : std::as_const( m_rows ) )
	{
		maxCount = qMax( maxCount, row.count );
	}

	int y = 4;
	for( const auto& row : std::as_const( m_rows ) )
	{
		const QRect labelRect( 4, y, labelWidth - 8, RowHeight - 6 );
		painter.setPen( palette().color( QPalette::WindowText ) );
		painter.drawText( labelRect, Qt::AlignVCenter | Qt::AlignLeft,
						  metrics.elidedText( row.label, Qt::ElideRight, labelRect.width() ) );

		const QRect backgroundRect( labelWidth, y + 3, barWidth, RowHeight - 12 );
		painter.setPen( Qt::NoPen );
		painter.setBrush( palette().color( QPalette::AlternateBase ) );
		painter.drawRoundedRect( backgroundRect, 3, 3 );
		if( row.count > 0 )
		{
			auto barRect = backgroundRect;
			barRect.setWidth( qMax( 4, barWidth * row.count / maxCount ) );
			painter.setBrush( row.color.isValid() ? row.color : DefaultColor );
			painter.drawRoundedRect( barRect, 3, 3 );
		}

		auto value = QStringLiteral("%1 (%2)").arg( row.count ).arg( percent( row.count, m_total ) );
		if( row.correct )
		{
			value += QStringLiteral(" ✓");
		}
		painter.setPen( row.correct ? CorrectColor.darker( 120 ) : palette().color( QPalette::WindowText ) );
		painter.drawText( QRect( labelWidth + barWidth + 8, y, valueWidth, RowHeight - 6 ),
						  Qt::AlignVCenter | Qt::AlignLeft, value );
		y += RowHeight;
	}
}



// ═════════════════════════════════════════════════════════════════════════════
//  results dialog
// ═════════════════════════════════════════════════════════════════════════════

SurveyResultsDialog::SurveyResultsDialog( const SurveyQuestion& question, const ComputerControlInterfaceList& computers,
										  const SendFunction& sendQuestion, const SendFunction& stopSurvey, QWidget* parent ) :
	QDialog( parent ),
	m_question( question ),
	m_computers( computers ),
	m_sendQuestion( sendQuestion ),
	m_stopSurvey( stopSurvey ),
	m_startTime( QDateTime::currentDateTime() ),
	m_timeLabel( new QLabel( this ) ),
	m_summaryLabel( new QLabel( this ) ),
	m_tabs( new QTabWidget( this ) ),
	m_chart( new SurveyChartWidget( this ) ),
	m_pendingLabel( new QLabel( this ) ),
	m_remindButton( new QPushButton( tr( "Send the question again" ), this ) ),
	m_closeSurveyButton( new QPushButton( tr( "Close survey" ), this ) ),
	m_folderButton( new QPushButton( tr( "Open results folder" ), this ) )
{
	setWindowTitle( tr( "Survey results" ) );
	setAttribute( Qt::WA_DeleteOnClose );
	resize( 760, 560 );

	m_entries.resize( m_computers.size() );
	for( const auto& computer : std::as_const( m_computers ) )
	{
		m_wasConnected.append( computer->state() == ComputerControlInterface::State::Connected );
	}

	auto questionLabel = new QLabel( m_question.text.isEmpty() ? m_question.typeName() : m_question.text, this );
	questionLabel->setWordWrap( true );
	questionLabel->setTextFormat( Qt::PlainText );
	auto questionFont = questionLabel->font();
	questionFont.setPointSizeF( questionFont.pointSizeF() * 1.3 );
	questionFont.setBold( true );
	questionLabel->setFont( questionFont );
	m_timeLabel->setFont( questionFont );

	auto headerLayout = new QHBoxLayout;
	headerLayout->addWidget( questionLabel, 1 );
	headerLayout->addWidget( m_timeLabel, 0, Qt::AlignTop );

	auto infoLabel = new QLabel( this );
	QStringList info{ m_question.typeName() };
	if( m_question.anonymous )
	{
		info.append( tr( "anonymous" ) );
	}
	if( m_question.timeLimit > 0 )
	{
		info.append( tr( "time limit %1" ).arg( duration( m_question.timeLimit ) ) );
	}
	infoLabel->setText( info.join( QStringLiteral(" · ") ) );
	infoLabel->setStyleSheet( QStringLiteral("color: gray;") );

	m_summaryLabel->setWordWrap( true );

	// chart tab
	auto chartPage = new QScrollArea( this );
	chartPage->setWidgetResizable( true );
	chartPage->setFrameShape( QFrame::NoFrame );
	auto chartContainer = new QWidget;
	auto chartLayout = new QVBoxLayout( chartContainer );
	chartLayout->addWidget( m_chart );
	chartLayout->addStretch();
	chartPage->setWidget( chartContainer );
	m_tabs->addTab( chartPage, tr( "Chart" ) );

	// per computer tab (never for anonymous surveys)
	if( m_question.anonymous == false )
	{
		m_table = new QTreeWidget( this );
		m_table->setRootIsDecorated( false );
		m_table->setSortingEnabled( true );
		m_table->setHeaderLabels( { tr( "Computer" ), tr( "User" ), tr( "Answer" ), tr( "Result" ), tr( "Time" ) } );
		m_table->setColumnWidth( 0, 150 );
		m_table->setColumnWidth( 1, 140 );
		m_table->setColumnWidth( 2, 240 );
		m_tabs->addTab( m_table, tr( "Per computer" ) );
	}

	// not answered tab
	auto pendingPage = new QWidget( this );
	auto pendingLayout = new QVBoxLayout( pendingPage );
	pendingLayout->addWidget( m_pendingLabel );
	if( m_question.anonymous == false )
	{
		m_pendingList = new QListWidget( pendingPage );
		pendingLayout->addWidget( m_pendingList );
	}
	else
	{
		pendingLayout->addStretch();
	}
	auto remindLayout = new QHBoxLayout;
	remindLayout->addWidget( m_remindButton );
	remindLayout->addStretch();
	pendingLayout->addLayout( remindLayout );
	m_tabs->addTab( pendingPage, tr( "Not answered" ) );

	auto exportButton = new QPushButton( tr( "Export (CSV for Excel)..." ), this );
	auto closeButton = new QPushButton( tr( "Close" ), this );
	m_folderButton->setEnabled( false );

	auto buttonLayout = new QHBoxLayout;
	buttonLayout->addWidget( exportButton );
	buttonLayout->addWidget( m_folderButton );
	buttonLayout->addStretch();
	buttonLayout->addWidget( m_closeSurveyButton );
	buttonLayout->addWidget( closeButton );

	auto layout = new QVBoxLayout( this );
	layout->addLayout( headerLayout );
	layout->addWidget( infoLabel );
	layout->addWidget( m_summaryLabel );
	layout->addWidget( m_tabs, 1 );
	layout->addLayout( buttonLayout );

	connect( m_remindButton, &QPushButton::clicked, this, &SurveyResultsDialog::remind );
	connect( m_closeSurveyButton, &QPushButton::clicked, this, &SurveyResultsDialog::closeSurvey );
	connect( exportButton, &QPushButton::clicked, this, &SurveyResultsDialog::exportCsv );
	connect( m_folderButton, &QPushButton::clicked, this, &SurveyResultsDialog::openFolder );
	connect( closeButton, &QPushButton::clicked, this, &SurveyResultsDialog::reject );

	m_elapsed.start();
	m_timer.setInterval( 500 );
	connect( &m_timer, &QTimer::timeout, this, &SurveyResultsDialog::updateTimer );
	m_timer.start();

	// computers which connect later (or again) get the question as well
	m_connectionTimer.setInterval( 2000 );
	connect( &m_connectionTimer, &QTimer::timeout, this, &SurveyResultsDialog::checkConnections );
	m_connectionTimer.start();

	updateTimer();
	updateView();
}



void SurveyResultsDialog::addAnswer( const ComputerControlInterface::Pointer& computer, const QJsonValue& answer )
{
	const auto index = m_computers.indexOf( computer );
	if( m_open == false || index < 0 || m_question.isValidAnswer( answer ) == false )
	{
		return;
	}

	auto& entry = m_entries[index];
	if( entry.answered && m_question.allowChange == false )
	{
		return;
	}
	entry.answered = true;
	entry.answer = answer;
	entry.seconds = m_elapsed.elapsed() / 1000;

	updateView();
}



void SurveyResultsDialog::closeSurvey()
{
	if( m_open == false )
	{
		return;
	}

	m_open = false;
	m_timer.stop();
	m_connectionTimer.stop();
	m_stopSurvey( m_computers );

	m_closeSurveyButton->setEnabled( false );
	m_closeSurveyButton->setText( tr( "Survey closed" ) );
	m_remindButton->setEnabled( false );
	m_timeLabel->setText( duration( m_elapsed.elapsed() / 1000 ) );

	saveResults();
	updateView();
}



void SurveyResultsDialog::reject()
{
	if( m_open )
	{
		const auto result = QMessageBox::question( this, windowTitle(),
			tr( "Close the survey? The students can not answer anymore and the results are saved." ),
			QMessageBox::Yes | QMessageBox::No );
		if( result != QMessageBox::Yes )
		{
			return;
		}
		closeSurvey();
	}
	QDialog::reject();
}



void SurveyResultsDialog::updateTimer()
{
	const auto seconds = m_elapsed.elapsed() / 1000;
	if( m_question.timeLimit > 0 )
	{
		const auto remaining = m_question.timeLimit - seconds;
		m_timeLabel->setText( duration( qMax<qint64>( 0, remaining ) ) );
		// a few seconds more for answers which are on their way
		if( remaining < -3 )
		{
			closeSurvey();
		}
		return;
	}
	m_timeLabel->setText( duration( seconds ) );
}



void SurveyResultsDialog::checkConnections()
{
	ComputerControlInterfaceList newlyConnected;
	for( int i = 0; i < m_computers.size(); ++i )
	{
		const auto connected = m_computers[i]->state() == ComputerControlInterface::State::Connected;
		if( connected && m_wasConnected[i] == false && m_entries[i].answered == false )
		{
			newlyConnected.append( m_computers[i] );
		}
		m_wasConnected[i] = connected;
	}

	if( newlyConnected.isEmpty() == false )
	{
		m_sendQuestion( newlyConnected );
	}
}



void SurveyResultsDialog::remind()
{
	ComputerControlInterfaceList pending;
	for( int i = 0; i < m_computers.size(); ++i )
	{
		if( m_entries[i].answered == false )
		{
			pending.append( m_computers[i] );
		}
	}
	if( pending.isEmpty() == false )
	{
		m_sendQuestion( pending );
	}
}



int SurveyResultsDialog::answeredCount() const
{
	int count = 0;
	for( const auto& entry : m_entries )
	{
		count += entry.answered ? 1 : 0;
	}
	return count;
}



QString SurveyResultsDialog::computerName( int index ) const
{
	return m_computers[index]->computer().displayName();
}



QString SurveyResultsDialog::userName( int index ) const
{
	const auto& computer = m_computers[index];
	return computer->userFullName().isEmpty() ? computer->userLoginName() : computer->userFullName();
}



QList<SurveyChartWidget::Row> SurveyResultsDialog::chartRows() const
{
	QList<SurveyChartWidget::Row> rows;

	if( m_question.isSingleIndex() || m_question.type == Type::MultipleChoice )
	{
		for( int i = 0; i < m_question.options.size(); ++i )
		{
			SurveyChartWidget::Row row;
			row.label = m_question.type == Type::Stars ? QString( i + 1, QChar( 0x2605 ) ) : m_question.optionLabel( i );
			row.correct = m_question.correctChoices.contains( i );
			row.color = row.correct ? CorrectColor : DefaultColor;
			if( m_question.type == Type::Understanding && i < 3 )
			{
				row.color = TrafficLightColors[i];
			}
			rows.append( row );
		}

		for( const auto& entry : m_entries )
		{
			if( entry.answered == false )
			{
				continue;
			}
			if( m_question.type == Type::MultipleChoice )
			{
				const auto array = entry.answer.toArray();
				for( const auto& choice : array )
				{
					const auto index = choice.toInt( -1 );
					if( index >= 0 && index < rows.size() )
					{
						++rows[index].count;
					}
				}
			}
			else
			{
				const auto index = entry.answer.toInt( -1 );
				if( index >= 0 && index < rows.size() )
				{
					++rows[index].count;
				}
			}
		}
		return rows;
	}

	// number and short text: group equal answers
	QList<Group> groups;
	QMap<QString, int> groupIndex;
	for( const auto& entry : m_entries )
	{
		if( entry.answered == false )
		{
			continue;
		}
		const auto text = m_question.answerText( entry.answer );
		const auto key = m_question.type == Type::Number ? text : SurveyQuestion::normalizedText( text );
		if( groupIndex.contains( key ) == false )
		{
			groupIndex[key] = groups.size();
			groups.append( { text, 0, m_question.correctness( entry.answer ) == 1 } );
		}
		++groups[groupIndex[key]].count;
	}
	std::stable_sort( groups.begin(), groups.end(), []( const Group& a, const Group& b ) { return a.count > b.count; } );

	int others = 0;
	for( int i = 0; i < groups.size(); ++i )
	{
		if( i < MaxChartRows )
		{
			rows.append( { groups[i].label, groups[i].count, groups[i].correct, groups[i].correct ? CorrectColor : DefaultColor } );
		}
		else
		{
			others += groups[i].count;
		}
	}
	if( others > 0 )
	{
		rows.append( { tr( "(other answers)" ), others, false, QColor( Qt::gray ) } );
	}
	return rows;
}



QString SurveyResultsDialog::summaryText() const
{
	const auto answered = answeredCount();
	QStringList parts{ tr( "Answered: <b>%1 / %2</b>" ).arg( answered ).arg( m_computers.size() ) };

	if( m_question.hasCorrectAnswer() && answered > 0 )
	{
		int correct = 0;
		for( const auto& entry : m_entries )
		{
			correct += entry.answered && m_question.correctness( entry.answer ) == 1 ? 1 : 0;
		}
		parts.append( tr( "Correct: <b>%1 (%2)</b>" ).arg( correct ).arg( percent( correct, answered ) ) );
	}

	QList<double> values;
	for( const auto& entry : m_entries )
	{
		if( entry.answered && ( m_question.isRating() || m_question.type == Type::Number ) )
		{
			values.append( m_question.type == Type::Number ? entry.answer.toDouble() : entry.answer.toInt() + 1 );
		}
	}
	if( values.isEmpty() == false )
	{
		std::sort( values.begin(), values.end() );
		double sum = 0;
		for( const auto value : std::as_const( values ) )
		{
			sum += value;
		}
		const auto size = values.size();
		const auto median = size % 2 ? values[size / 2] : ( values[size / 2 - 1] + values[size / 2] ) / 2;
		parts.append( tr( "Average: <b>%1</b>" ).arg( QLocale().toString( sum / size, 'f', 2 ) ) );
		parts.append( tr( "Median: <b>%1</b>" ).arg( SurveyQuestion::formatNumber( median ) ) );
		if( m_question.type == Type::Number )
		{
			parts.append( tr( "Smallest: <b>%1</b>" ).arg( SurveyQuestion::formatNumber( values.first() ) ) );
			parts.append( tr( "Largest: <b>%1</b>" ).arg( SurveyQuestion::formatNumber( values.last() ) ) );
		}
	}

	if( m_open == false && m_savedFile.isEmpty() == false )
	{
		parts.append( tr( "Results saved: %1" ).arg( QDir::toNativeSeparators( m_savedFile ).toHtmlEscaped() ) );
	}

	return parts.join( QStringLiteral(" &nbsp;·&nbsp; ") );
}



void SurveyResultsDialog::updateView()
{
	const auto answered = answeredCount();
	m_summaryLabel->setText( summaryText() );
	m_chart->setRows( chartRows(), answered );

	if( m_table )
	{
		m_table->setSortingEnabled( false );
		m_table->clear();
		for( int i = 0; i < m_computers.size(); ++i )
		{
			const auto& entry = m_entries[i];
			if( entry.answered == false )
			{
				continue;
			}
			auto item = new QTreeWidgetItem( m_table );
			item->setText( 0, computerName( i ) );
			item->setText( 1, userName( i ) );
			item->setText( 2, m_question.answerText( entry.answer ) );
			const auto correctness = m_question.correctness( entry.answer );
			if( correctness >= 0 )
			{
				item->setText( 3, correctness ? tr( "Correct" ) : tr( "Wrong" ) );
				item->setForeground( 3, correctness ? CorrectColor.darker( 130 ) : QColor( 0xf4, 0x43, 0x36 ) );
			}
			item->setText( 4, duration( entry.seconds ) );
		}
		m_table->setSortingEnabled( true );
	}

	const auto pending = m_computers.size() - answered;
	m_tabs->setTabText( m_tabs->count() - 1, tr( "Not answered (%1)" ).arg( pending ) );
	m_pendingLabel->setText( pending > 0 ? tr( "%1 computers have not answered yet." ).arg( pending ) :
										   tr( "All computers have answered." ) );
	m_remindButton->setEnabled( m_open && pending > 0 );
	if( m_pendingList )
	{
		m_pendingList->clear();
		for( int i = 0; i < m_computers.size(); ++i )
		{
			if( m_entries[i].answered == false )
			{
				const auto user = userName( i );
				m_pendingList->addItem( user.isEmpty() ? computerName( i ) : QStringLiteral("%1 (%2)").arg( computerName( i ), user ) );
			}
		}
	}
}



QJsonObject SurveyResultsDialog::resultsJson() const
{
	QJsonArray statistics;
	const auto rows = chartRows();
	for( const auto& row : rows )
	{
		statistics.append( QJsonObject{ { QStringLiteral("answer"), row.label },
										{ QStringLiteral("count"), row.count },
										{ QStringLiteral("correct"), row.correct } } );
	}

	QJsonArray answers;
	if( m_question.anonymous )
	{
		// only the answers, in random order
		QList<int> order;
		for( int i = 0; i < m_entries.size(); ++i )
		{
			if( m_entries[i].answered )
			{
				order.append( i );
			}
		}
		std::shuffle( order.begin(), order.end(), *QRandomGenerator::global() );
		for( const auto i : std::as_const( order ) )
		{
			answers.append( QJsonObject{ { QStringLiteral("answer"), m_entries[i].answer },
										 { QStringLiteral("answerText"), m_question.answerText( m_entries[i].answer ) } } );
		}
	}
	else
	{
		for( int i = 0; i < m_entries.size(); ++i )
		{
			const auto& entry = m_entries[i];
			QJsonObject object{ { QStringLiteral("computer"), computerName( i ) },
								{ QStringLiteral("user"), userName( i ) },
								{ QStringLiteral("answered"), entry.answered } };
			if( entry.answered )
			{
				object[QStringLiteral("answer")] = entry.answer;
				object[QStringLiteral("answerText")] = m_question.answerText( entry.answer );
				object[QStringLiteral("correct")] = m_question.correctness( entry.answer );
				object[QStringLiteral("seconds")] = entry.seconds;
			}
			answers.append( object );
		}
	}

	auto question = m_question.toJson();
	question.remove( QStringLiteral("id") );

	return QJsonObject{
		{ QStringLiteral("question"), question },
		{ QStringLiteral("date"), m_startTime.toString( Qt::ISODate ) },
		{ QStringLiteral("computers"), int( m_computers.size() ) },
		{ QStringLiteral("answered"), answeredCount() },
		{ QStringLiteral("statistics"), statistics },
		{ QStringLiteral("answers"), answers },
	};
}



QString SurveyResultsDialog::csvText() const
{
	QString text;
	const auto answered = answeredCount();

	text += csvLine( { tr( "Question" ), m_question.text } );
	text += csvLine( { tr( "Type" ), m_question.typeName() } );
	text += csvLine( { tr( "Date" ), m_startTime.toString( QStringLiteral("dd/MM/yyyy HH:mm") ) } );
	text += csvLine( { tr( "Answered" ), QStringLiteral("%1 / %2").arg( answered ).arg( m_computers.size() ) } );
	text += QStringLiteral("\r\n");

	text += csvLine( { tr( "Answer" ), tr( "Count" ), tr( "Percent" ), tr( "Correct" ) } );
	const auto rows = chartRows();
	for( const auto& row : rows )
	{
		text += csvLine( { row.label, QString::number( row.count ), percent( row.count, answered ),
						   row.correct ? tr( "yes" ) : QString{} } );
	}
	text += QStringLiteral("\r\n");

	if( m_question.anonymous )
	{
		const auto answers = resultsJson().value( QStringLiteral("answers") ).toArray();
		text += csvLine( { tr( "Answers (anonymous, random order)" ) } );
		for( const auto& answer : answers )
		{
			text += csvLine( { answer.toObject().value( QStringLiteral("answerText") ).toString() } );
		}
		return text;
	}

	text += csvLine( { tr( "Computer" ), tr( "User" ), tr( "Answer" ), tr( "Result" ), tr( "Time" ) } );
	for( int i = 0; i < m_entries.size(); ++i )
	{
		const auto& entry = m_entries[i];
		if( entry.answered == false )
		{
			text += csvLine( { computerName( i ), userName( i ), tr( "(not answered)" ) } );
			continue;
		}
		const auto correctness = m_question.correctness( entry.answer );
		text += csvLine( { computerName( i ), userName( i ), m_question.answerText( entry.answer ),
						   correctness < 0 ? QString{} : ( correctness ? tr( "Correct" ) : tr( "Wrong" ) ),
						   duration( entry.seconds ) } );
	}
	return text;
}



void SurveyResultsDialog::saveResults()
{
	const QString directory = QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) +
						   QStringLiteral("/ITLabControl/") + tr( "Surveys" ) + QLatin1Char('/') +
						   m_startTime.toString( QStringLiteral("yyyy-MM-dd") );
	if( QDir().mkpath( directory ) == false )
	{
		return;
	}

	const QString baseName = directory + QLatin1Char('/') + m_startTime.toString( QStringLiteral("HH-mm-ss") );

	QFile jsonFile( baseName + QStringLiteral(".json") );
	if( jsonFile.open( QFile::WriteOnly ) )
	{
		jsonFile.write( QJsonDocument( resultsJson() ).toJson() );
	}

	QFile csvFile( baseName + QStringLiteral(".csv") );
	if( csvFile.open( QFile::WriteOnly ) )
	{
		// UTF-8 with BOM so that Excel shows Vietnamese characters correctly
		csvFile.write( "\xEF\xBB\xBF" );
		csvFile.write( csvText().toUtf8() );
		m_savedFile = csvFile.fileName();
		m_folderButton->setEnabled( true );
	}
}



void SurveyResultsDialog::exportCsv()
{
	const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export survey results" ),
		QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) +
			QStringLiteral("/KhaoSat_%1.csv").arg( m_startTime.toString( QStringLiteral("yyyy-MM-dd_HH-mm") ) ),
		tr( "CSV files (*.csv)" ) );
	if( fileName.isEmpty() )
	{
		return;
	}

	QFile file( fileName );
	if( file.open( QFile::WriteOnly ) == false )
	{
		QMessageBox::critical( this, tr( "Export survey results" ), tr( "The file could not be written." ) );
		return;
	}
	file.write( "\xEF\xBB\xBF" );
	file.write( csvText().toUtf8() );
}



void SurveyResultsDialog::openFolder()
{
	if( m_savedFile.isEmpty() == false )
	{
		QDesktopServices::openUrl( QUrl::fromLocalFile( QFileInfo( m_savedFile ).absolutePath() ) );
	}
}
