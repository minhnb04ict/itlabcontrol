/*
 * SurveyAnswerWindow.cpp - window on the student computer to answer a quick survey (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <QCheckBox>
#include <QCloseEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#include "SurveyAnswerWindow.h"


namespace {

const char* const TrafficLightStyles[] = {
	"QPushButton { background-color: #4caf50; color: white; }",
	"QPushButton { background-color: #ffc107; color: black; }",
	"QPushButton { background-color: #f44336; color: white; }",
};

QFont scaledFont( const QFont& font, double factor, bool bold = false )
{
	QFont result( font );
	result.setPointSizeF( font.pointSizeF() * factor );
	result.setBold( bold );
	return result;
}

}



SurveyAnswerWindow::SurveyAnswerWindow( const SurveyQuestion& question, const SubmitFunction& submit ) :
	QWidget( nullptr, Qt::Window | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint | Qt::WindowTitleHint ),
	m_question( question ),
	m_submitFunction( submit )
{
	setWindowTitle( tr( "Question from the teacher" ) );
	setAttribute( Qt::WA_DeleteOnClose );
	setAttribute( Qt::WA_QuitOnClose, false );
	setMinimumWidth( 460 );
	setMaximumWidth( 760 );

	auto layout = new QVBoxLayout( this );
	layout->setSpacing( 10 );

	auto headerLayout = new QHBoxLayout;
	auto headerLabel = new QLabel( m_question.anonymous ?
									   tr( "Anonymous survey - the teacher can not see who answered what." ) :
									   tr( "Question from the teacher" ), this );
	headerLabel->setWordWrap( true );
	if( m_question.anonymous )
	{
		headerLabel->setStyleSheet( QStringLiteral("color: #2e7d32; font-weight: bold;") );
	}
	m_countdownLabel = new QLabel( this );
	m_countdownLabel->setFont( scaledFont( font(), 1.2, true ) );
	headerLayout->addWidget( headerLabel, 1 );
	headerLayout->addWidget( m_countdownLabel );
	layout->addLayout( headerLayout );

	auto questionLabel = new QLabel( m_question.text.isEmpty() ? tr( "Choose your answer:" ) : m_question.text, this );
	questionLabel->setWordWrap( true );
	questionLabel->setTextFormat( Qt::PlainText );
	questionLabel->setFont( scaledFont( font(), 1.5, true ) );
	layout->addWidget( questionLabel );

	m_answerArea = new QWidget( this );
	buildAnswerArea( new QVBoxLayout( m_answerArea ) );
	m_answerArea->layout()->setContentsMargins( 0, 0, 0, 0 );
	layout->addWidget( m_answerArea );

	m_sentArea = new QWidget( this );
	auto sentLayout = new QVBoxLayout( m_sentArea );
	sentLayout->setContentsMargins( 0, 0, 0, 0 );
	m_sentLabel = new QLabel( m_sentArea );
	m_sentLabel->setWordWrap( true );
	m_sentLabel->setTextFormat( Qt::PlainText );
	m_sentLabel->setFont( scaledFont( font(), 1.2, true ) );
	m_sentLabel->setStyleSheet( QStringLiteral("color: #2e7d32;") );
	auto sentButtons = new QHBoxLayout;
	m_changeButton = new QPushButton( tr( "Change answer" ), m_sentArea );
	auto closeButton = new QPushButton( tr( "Close" ), m_sentArea );
	sentButtons->addStretch();
	sentButtons->addWidget( m_changeButton );
	sentButtons->addWidget( closeButton );
	sentLayout->addWidget( m_sentLabel );
	sentLayout->addLayout( sentButtons );
	layout->addWidget( m_sentArea );
	m_sentArea->hide();

	connect( m_changeButton, &QPushButton::clicked, this, &SurveyAnswerWindow::showAnswerArea );
	connect( closeButton, &QPushButton::clicked, this, &QWidget::close );

	m_elapsed.start();
	if( m_question.timeLimit > 0 )
	{
		m_countdownTimer.setInterval( 500 );
		connect( &m_countdownTimer, &QTimer::timeout, this, &SurveyAnswerWindow::updateCountdown );
		m_countdownTimer.start();
		updateCountdown();
	}

	adjustSize();
	if( const auto screen = QGuiApplication::primaryScreen() )
	{
		const auto area = screen->availableGeometry();
		move( area.center().x() - width() / 2, area.top() + area.height() / 8 );
	}
}



void SurveyAnswerWindow::finish()
{
	m_allowClose = true;
	close();
}



void SurveyAnswerWindow::closeEvent( QCloseEvent* event )
{
	// the question has to be answered (or the teacher closes the survey)
	if( m_allowClose == false && m_answered == false )
	{
		event->ignore();
		return;
	}
	QWidget::closeEvent( event );
}



void SurveyAnswerWindow::buildAnswerArea( QLayout* layout )
{
	using Type = SurveyQuestion::Type;
	const auto bigFont = scaledFont( font(), 1.4, true );

	if( m_question.isSingleIndex() )
	{
		bool longLabels = false;
		for( int i = 0; i < m_question.options.size(); ++i )
		{
			longLabels |= m_question.optionLabel( i ).size() > 14;
		}

		auto grid = new QGridLayout;
		grid->setSpacing( 8 );
		const auto columns = longLabels ? 1 : qMin( 5, int( m_question.options.size() ) );
		for( int i = 0; i < m_question.options.size(); ++i )
		{
			auto text = m_question.optionLabel( i );
			if( m_question.type == Type::Stars )
			{
				text = QString( i + 1, QChar( 0x2605 ) );
			}
			auto button = new QPushButton( text, m_answerArea );
			button->setFont( bigFont );
			button->setMinimumHeight( 54 );
			button->setMinimumWidth( 70 );
			if( m_question.type == Type::Understanding && i < 3 )
			{
				button->setStyleSheet( QString::fromLatin1( TrafficLightStyles[i] ) );
			}
			if( m_question.type == Type::Stars )
			{
				button->setStyleSheet( QStringLiteral("QPushButton { color: #f9a825; }") );
			}
			connect( button, &QPushButton::clicked, this, [this, i]() { submit( QJsonValue( i ) ); } );
			grid->addWidget( button, i / columns, i % columns );
			m_choiceButtons.append( button );
		}
		static_cast<QVBoxLayout*>( layout )->addLayout( grid );

		if( m_question.type == Type::Scale && ( m_question.lowLabel.isEmpty() == false || m_question.highLabel.isEmpty() == false ) )
		{
			auto labels = new QHBoxLayout;
			auto low = new QLabel( QStringLiteral("1 = ") + m_question.lowLabel, m_answerArea );
			auto high = new QLabel( QString::number( m_question.options.size() ) + QStringLiteral(" = ") + m_question.highLabel, m_answerArea );
			low->setTextFormat( Qt::PlainText );
			high->setTextFormat( Qt::PlainText );
			labels->addWidget( low );
			labels->addStretch();
			labels->addWidget( high );
			static_cast<QVBoxLayout*>( layout )->addLayout( labels );
		}
		return;
	}

	m_submitButton = new QPushButton( tr( "Send answer" ), m_answerArea );
	m_submitButton->setFont( scaledFont( font(), 1.2, true ) );
	m_submitButton->setMinimumHeight( 44 );

	if( m_question.type == Type::MultipleChoice )
	{
		auto hint = new QLabel( m_question.maxSelections > 0 ?
									tr( "Select up to %1 answers." ).arg( m_question.maxSelections ) :
									tr( "Select all correct answers." ), m_answerArea );
		layout->addWidget( hint );
		for( int i = 0; i < m_question.options.size(); ++i )
		{
			auto checkBox = new QCheckBox( m_question.optionLabel( i ), m_answerArea );
			checkBox->setFont( bigFont );
			layout->addWidget( checkBox );
			m_choiceButtons.append( checkBox );
			connect( checkBox, &QCheckBox::toggled, this, [this]() {
				if( m_question.maxSelections <= 0 )
				{
					return;
				}
				int checked = 0;
				for( auto button : std::as_const( m_choiceButtons ) )
				{
					checked += button->isChecked() ? 1 : 0;
				}
				for( auto button : std::as_const( m_choiceButtons ) )
				{
					button->setEnabled( button->isChecked() || checked < m_question.maxSelections );
				}
			} );
		}
		connect( m_submitButton, &QPushButton::clicked, this, &SurveyAnswerWindow::submitMultipleChoice );
	}
	else if( m_question.type == Type::Number )
	{
		m_numberEdit = new QLineEdit( m_answerArea );
		m_numberEdit->setFont( bigFont );
		m_numberEdit->setPlaceholderText( m_question.hasRange ?
			tr( "Number from %1 to %2" ).arg( SurveyQuestion::formatNumber( m_question.minimum ),
											  SurveyQuestion::formatNumber( m_question.maximum ) ) :
			tr( "Enter a number" ) );
		layout->addWidget( m_numberEdit );
		connect( m_numberEdit, &QLineEdit::returnPressed, this, &SurveyAnswerWindow::submitNumber );
		connect( m_submitButton, &QPushButton::clicked, this, &SurveyAnswerWindow::submitNumber );
	}
	else
	{
		m_textEdit = new QLineEdit( m_answerArea );
		m_textEdit->setFont( bigFont );
		m_textEdit->setMaxLength( SurveyQuestion::MaxTextLength );
		m_textEdit->setPlaceholderText( tr( "Type your answer" ) );
		layout->addWidget( m_textEdit );
		connect( m_textEdit, &QLineEdit::returnPressed, this, &SurveyAnswerWindow::submitText );
		connect( m_submitButton, &QPushButton::clicked, this, &SurveyAnswerWindow::submitText );
	}

	auto buttons = new QHBoxLayout;
	buttons->addStretch();
	buttons->addWidget( m_submitButton );
	static_cast<QVBoxLayout*>( layout )->addLayout( buttons );
}



void SurveyAnswerWindow::submitMultipleChoice()
{
	QJsonArray selected;
	for( int i = 0; i < m_choiceButtons.size(); ++i )
	{
		if( m_choiceButtons[i]->isChecked() )
		{
			selected.append( i );
		}
	}
	if( selected.isEmpty() )
	{
		m_submitButton->setText( tr( "Select at least one answer" ) );
		return;
	}
	submit( selected );
}



void SurveyAnswerWindow::submitNumber()
{
	auto text = m_numberEdit->text().trimmed();
	bool ok = false;
	auto value = QLocale().toDouble( text, &ok );
	if( ok == false )
	{
		value = text.replace( QLatin1Char(','), QLatin1Char('.') ).toDouble( &ok );
	}
	if( ok == false || m_question.isValidAnswer( value ) == false )
	{
		m_numberEdit->selectAll();
		m_numberEdit->setFocus();
		m_numberEdit->setStyleSheet( QStringLiteral("QLineEdit { border: 2px solid #f44336; }") );
		return;
	}
	m_numberEdit->setStyleSheet( {} );
	submit( value );
}



void SurveyAnswerWindow::submitText()
{
	const auto text = m_textEdit->text().simplified();
	if( text.isEmpty() )
	{
		m_textEdit->setFocus();
		return;
	}
	submit( text );
}



void SurveyAnswerWindow::submit( const QJsonValue& answer )
{
	if( m_question.isValidAnswer( answer ) == false || ( m_answered && m_question.allowChange == false ) )
	{
		return;
	}

	m_submitFunction( answer );
	m_answered = true;

	auto answerText = m_question.answerText( answer );
	if( m_question.type == SurveyQuestion::Type::Stars )
	{
		answerText = QString( answer.toInt() + 1, QChar( 0x2605 ) );
	}
	m_sentLabel->setText( tr( "Your answer was sent: %1" ).arg( answerText ) );
	showSentArea();
}



void SurveyAnswerWindow::showAnswerArea()
{
	m_sentArea->hide();
	m_answerArea->show();
	adjustSize();
}



void SurveyAnswerWindow::showSentArea()
{
	m_answerArea->hide();
	m_changeButton->setVisible( m_question.allowChange );
	m_sentArea->show();
	adjustSize();
}



void SurveyAnswerWindow::updateCountdown()
{
	const auto remaining = m_question.timeLimit - int( m_elapsed.elapsed() / 1000 );
	if( remaining > 0 )
	{
		m_countdownLabel->setText( QStringLiteral("%1:%2").arg( remaining / 60, 2, 10, QLatin1Char('0') )
															.arg( remaining % 60, 2, 10, QLatin1Char('0') ) );
		m_countdownLabel->setStyleSheet( remaining <= 10 ? QStringLiteral("color: #f44336;") : QString{} );
		return;
	}

	m_countdownTimer.stop();
	m_countdownLabel->setText( tr( "Time is up" ) );
	m_answerArea->setEnabled( false );
	m_changeButton->setEnabled( false );
	m_allowClose = true;
	QTimer::singleShot( 3000, this, &QWidget::close );
}
