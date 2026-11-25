/*
 * SurveyComposeDialog.cpp - compose a quick survey question in the Teacher Console (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QUuid>
#include <QVBoxLayout>

#include "SurveyComposeDialog.h"


SurveyQuestion SurveyComposeDialog::s_lastQuestion;
bool SurveyComposeDialog::s_hasLastQuestion = false;


namespace {

using Type = SurveyQuestion::Type;

// page of the stacked widget for each question type
int pageIndex( Type type )
{
	switch( type )
	{
	case Type::TrueFalse: return 0;
	case Type::YesNo: return 1;
	case Type::SingleChoice:
	case Type::MultipleChoice: return 2;
	case Type::Scale: return 3;
	case Type::Agreement: return 4;
	case Type::Number: return 6;
	case Type::ShortText: return 7;
	default: break;
	}
	return 5; // nothing to set
}

const QList<int> TimeLimits{ 0, 15, 30, 60, 120, 180, 300, 600 };

}



SurveyComposeDialog::SurveyComposeDialog( SurveyQuestion::Type type, int computerCount, QWidget* parent ) :
	QDialog( parent ),
	m_textEdit( new QPlainTextEdit( this ) ),
	m_typeCombo( new QComboBox( this ) ),
	m_pages( new QStackedWidget( this ) ),
	m_timeCombo( new QComboBox( this ) ),
	m_allowChangeCheck( new QCheckBox( tr( "Students can change their answer" ), this ) ),
	m_anonymousCheck( new QCheckBox( tr( "Anonymous (nobody can see who answered what)" ), this ) )
{
	setWindowTitle( tr( "Quick survey" ) + QStringLiteral(" - ") + SurveyQuestion::typeName( type ) );
	setMinimumWidth( 620 );

	m_textEdit->setPlaceholderText( tr( "Question (may stay empty if the question is shown on the board or projector)" ) );
	m_textEdit->setMaximumHeight( 80 );
	m_textEdit->setTabChangesFocus( true );

	for( int i = 0; i < int(Type::Count); ++i )
	{
		m_typeCombo->addItem( SurveyQuestion::typeName( Type(i) ), i );
	}

	// page 0: true / false
	{
		auto page = new QWidget;
		auto form = new QFormLayout( page );
		m_trueFalseCorrect = new QComboBox( page );
		m_trueFalseCorrect->addItems( { tr( "(not set)" ), tr( "True" ), tr( "False" ) } );
		form->addRow( tr( "Correct answer:" ), m_trueFalseCorrect );
		m_pages->addWidget( page );
	}
	// page 1: yes / no
	{
		auto page = new QWidget;
		auto layout = new QVBoxLayout( page );
		m_notSureCheck = new QCheckBox( tr( "Add the answer \"Not sure\"" ), page );
		layout->addWidget( m_notSureCheck );
		layout->addStretch();
		m_pages->addWidget( page );
	}
	// page 2: single / multiple choice
	{
		auto page = new QWidget;
		auto layout = new QVBoxLayout( page );
		auto countLayout = new QHBoxLayout;
		m_choiceCount = new QSpinBox( page );
		m_choiceCount->setRange( 2, SurveyQuestion::MaxOptions );
		m_choiceCount->setValue( 4 );
		m_maxSelectionsLabel = new QLabel( tr( "Max. selections:" ), page );
		m_maxSelections = new QSpinBox( page );
		m_maxSelections->setRange( 0, SurveyQuestion::MaxOptions );
		m_maxSelections->setSpecialValueText( tr( "no limit" ) );
		countLayout->addWidget( new QLabel( tr( "Number of answers:" ), page ) );
		countLayout->addWidget( m_choiceCount );
		countLayout->addSpacing( 20 );
		countLayout->addWidget( m_maxSelectionsLabel );
		countLayout->addWidget( m_maxSelections );
		countLayout->addStretch();
		layout->addLayout( countLayout );

		auto hint = new QLabel( tr( "The content of the answers may stay empty, the students then only choose the letter. "
									"Tick the correct answer to see the percentage of correct answers (optional)." ), page );
		hint->setWordWrap( true );
		layout->addWidget( hint );

		m_choiceGrid = new QGridLayout;
		for( int i = 0; i < SurveyQuestion::MaxOptions; ++i )
		{
			auto letterLabel = new QLabel( SurveyQuestion::letter( i ), page );
			letterLabel->setStyleSheet( QStringLiteral("font-weight: bold;") );
			auto edit = new QLineEdit( page );
			edit->setPlaceholderText( tr( "(optional)" ) );
			auto correct = new QCheckBox( tr( "correct" ), page );
			m_choiceGrid->addWidget( letterLabel, i, 0 );
			m_choiceGrid->addWidget( edit, i, 1 );
			m_choiceGrid->addWidget( correct, i, 2 );
			m_choiceEdits.append( edit );
			m_choiceCorrect.append( correct );

			// single choice: only one correct answer
			connect( correct, &QCheckBox::toggled, this, [this, correct]( bool checked ) {
				if( checked && Type( m_typeCombo->currentData().toInt() ) != Type::MultipleChoice )
				{
					for( auto other : std::as_const( m_choiceCorrect ) )
					{
						if( other != correct )
						{
							other->setChecked( false );
						}
					}
				}
			} );
		}
		layout->addLayout( m_choiceGrid );
		layout->addStretch();
		m_pages->addWidget( page );

		connect( m_choiceCount, &QSpinBox::valueChanged, this, &SurveyComposeDialog::updateChoiceRows );
	}
	// page 3: scale
	{
		auto page = new QWidget;
		auto form = new QFormLayout( page );
		m_scaleCount = new QSpinBox( page );
		m_scaleCount->setRange( 2, SurveyQuestion::MaxOptions );
		m_scaleCount->setValue( 5 );
		m_lowLabel = new QLineEdit( page );
		m_lowLabel->setPlaceholderText( tr( "e.g. very difficult" ) );
		m_highLabel = new QLineEdit( page );
		m_highLabel->setPlaceholderText( tr( "e.g. very easy" ) );
		form->addRow( tr( "Highest value n:" ), m_scaleCount );
		form->addRow( tr( "Label of 1:" ), m_lowLabel );
		form->addRow( tr( "Label of n:" ), m_highLabel );
		m_pages->addWidget( page );
	}
	// page 4: agreement
	{
		auto page = new QWidget;
		auto form = new QFormLayout( page );
		SurveyQuestion defaults;
		defaults.type = Type::Agreement;
		setDefaultOptions( defaults );
		for( int i = 0; i < defaults.options.size(); ++i )
		{
			auto edit = new QLineEdit( defaults.options[i], page );
			form->addRow( QStringLiteral("%1:").arg( i + 1 ), edit );
			m_agreementEdits.append( edit );
		}
		m_pages->addWidget( page );
	}
	// page 5: nothing to set (stars, understanding)
	{
		auto page = new QWidget;
		auto layout = new QVBoxLayout( page );
		auto label = new QLabel( tr( "The answer buttons are fixed for this type." ), page );
		label->setWordWrap( true );
		layout->addWidget( label );
		layout->addStretch();
		m_pages->addWidget( page );
	}
	// page 6: number
	{
		auto page = new QWidget;
		auto grid = new QGridLayout( page );
		auto makeSpin = [page]() {
			auto spin = new QDoubleSpinBox( page );
			spin->setRange( -1e9, 1e9 );
			spin->setDecimals( 3 );
			return spin;
		};
		m_rangeCheck = new QCheckBox( tr( "Allowed range from" ), page );
		m_minimum = makeSpin();
		m_maximum = makeSpin();
		m_maximum->setValue( 100 );
		m_correctNumberCheck = new QCheckBox( tr( "Correct answer" ), page );
		m_correctNumber = makeSpin();
		m_tolerance = makeSpin();
		m_tolerance->setMinimum( 0 );
		grid->addWidget( m_rangeCheck, 0, 0 );
		grid->addWidget( m_minimum, 0, 1 );
		grid->addWidget( new QLabel( tr( "to" ), page ), 0, 2 );
		grid->addWidget( m_maximum, 0, 3 );
		grid->addWidget( m_correctNumberCheck, 1, 0 );
		grid->addWidget( m_correctNumber, 1, 1 );
		grid->addWidget( new QLabel( tr( "±" ), page ), 1, 2 );
		grid->addWidget( m_tolerance, 1, 3 );
		grid->setRowStretch( 2, 1 );
		m_pages->addWidget( page );
	}
	// page 7: short text
	{
		auto page = new QWidget;
		auto form = new QFormLayout( page );
		m_correctTexts = new QLineEdit( page );
		m_correctTexts->setPlaceholderText( tr( "optional, several answers separated by ;" ) );
		form->addRow( tr( "Correct answer:" ), m_correctTexts );
		auto hint = new QLabel( tr( "Upper/lower case and spaces are ignored. The students can type up to %1 characters." )
									.arg( SurveyQuestion::MaxTextLength ), page );
		hint->setWordWrap( true );
		form->addRow( hint );
		m_pages->addWidget( page );
	}

	m_timeCombo->addItem( tr( "No time limit" ), 0 );
	for( const auto seconds : TimeLimits )
	{
		if( seconds > 0 )
		{
			m_timeCombo->addItem( seconds < 60 ? tr( "%1 seconds" ).arg( seconds ) : tr( "%1 minutes" ).arg( seconds / 60 ), seconds );
		}
	}
	m_allowChangeCheck->setChecked( true );

	auto optionsBox = new QGroupBox( tr( "Options" ), this );
	auto optionsLayout = new QFormLayout( optionsBox );
	optionsLayout->addRow( tr( "Time to answer:" ), m_timeCombo );
	optionsLayout->addRow( m_allowChangeCheck );
	optionsLayout->addRow( m_anonymousCheck );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Cancel, this );
	auto sendButton = buttons->addButton( tr( "Send to %1 computers" ).arg( computerCount ), QDialogButtonBox::AcceptRole );
	sendButton->setDefault( true );

	auto form = new QFormLayout;
	form->addRow( tr( "Question:" ), m_textEdit );
	form->addRow( tr( "Type:" ), m_typeCombo );

	auto layout = new QVBoxLayout( this );
	layout->addLayout( form );
	layout->addWidget( m_pages );
	layout->addWidget( optionsBox );
	layout->addWidget( buttons );

	connect( m_typeCombo, &QComboBox::currentIndexChanged, this, &SurveyComposeDialog::updatePage );
	connect( buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
	connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );

	// type chosen in the menu, options of the last question
	m_typeCombo->setCurrentIndex( int( type ) );
	if( s_hasLastQuestion )
	{
		if( s_lastQuestion.type == type && ( type == Type::SingleChoice || type == Type::MultipleChoice ) )
		{
			m_choiceCount->setValue( s_lastQuestion.options.size() );
		}
		m_timeCombo->setCurrentIndex( qMax( 0, m_timeCombo->findData( s_lastQuestion.timeLimit ) ) );
		m_allowChangeCheck->setChecked( s_lastQuestion.allowChange );
		m_anonymousCheck->setChecked( s_lastQuestion.anonymous );
	}

	updatePage();
	updateChoiceRows();
	m_textEdit->setFocus();
}



void SurveyComposeDialog::updatePage()
{
	const auto type = Type( m_typeCombo->currentData().toInt() );
	m_pages->setCurrentIndex( pageIndex( type ) );
	const auto multiple = type == Type::MultipleChoice;
	m_maxSelectionsLabel->setVisible( multiple );
	m_maxSelections->setVisible( multiple );

	if( multiple == false )
	{
		// keep only the first correct answer
		bool found = false;
		for( auto correct : std::as_const( m_choiceCorrect ) )
		{
			if( correct->isChecked() )
			{
				correct->setChecked( found == false );
				found = true;
			}
		}
	}
}



void SurveyComposeDialog::updateChoiceRows()
{
	const auto count = m_choiceCount->value();
	for( int i = 0; i < SurveyQuestion::MaxOptions; ++i )
	{
		for( int column = 0; column < 3; ++column )
		{
			if( auto item = m_choiceGrid->itemAtPosition( i, column ); item && item->widget() )
			{
				item->widget()->setVisible( i < count );
			}
		}
		if( i >= count )
		{
			m_choiceCorrect[i]->setChecked( false );
		}
	}
	m_maxSelections->setMaximum( count );
}



QStringList SurveyComposeDialog::agreementLabels() const
{
	QStringList labels;
	for( const auto edit : m_agreementEdits )
	{
		labels.append( edit->text().trimmed().isEmpty() ? edit->placeholderText() : edit->text().trimmed() );
	}
	return labels;
}



SurveyQuestion SurveyComposeDialog::question() const
{
	SurveyQuestion question;
	question.id = QUuid::createUuid().toString( QUuid::WithoutBraces );
	question.type = Type( m_typeCombo->currentData().toInt() );
	question.text = m_textEdit->toPlainText().trimmed();
	question.timeLimit = m_timeCombo->currentData().toInt();
	question.allowChange = m_allowChangeCheck->isChecked();
	question.anonymous = m_anonymousCheck->isChecked();

	switch( question.type )
	{
	case Type::TrueFalse:
		setDefaultOptions( question );
		if( m_trueFalseCorrect->currentIndex() > 0 )
		{
			question.correctChoices = { m_trueFalseCorrect->currentIndex() - 1 };
		}
		break;
	case Type::YesNo:
		setDefaultOptions( question, m_notSureCheck->isChecked() ? 3 : 2 );
		break;
	case Type::SingleChoice:
	case Type::MultipleChoice:
		for( int i = 0; i < m_choiceCount->value(); ++i )
		{
			question.options.append( SurveyQuestion::letter( i ) );
			question.optionTexts.append( m_choiceEdits[i]->text().trimmed() );
			if( m_choiceCorrect[i]->isChecked() )
			{
				question.correctChoices.append( i );
			}
		}
		if( question.type == Type::MultipleChoice )
		{
			question.maxSelections = m_maxSelections->value();
		}
		break;
	case Type::Scale:
		setDefaultOptions( question, m_scaleCount->value() );
		question.lowLabel = m_lowLabel->text().trimmed();
		question.highLabel = m_highLabel->text().trimmed();
		break;
	case Type::Agreement:
		question.options = agreementLabels();
		break;
	case Type::Number:
		question.hasRange = m_rangeCheck->isChecked();
		question.minimum = qMin( m_minimum->value(), m_maximum->value() );
		question.maximum = qMax( m_minimum->value(), m_maximum->value() );
		question.hasCorrectNumber = m_correctNumberCheck->isChecked();
		question.correctNumber = m_correctNumber->value();
		question.tolerance = m_tolerance->value();
		break;
	case Type::ShortText:
		for( const auto& text : m_correctTexts->text().split( QLatin1Char(';') ) )
		{
			if( text.trimmed().isEmpty() == false )
			{
				question.correctTexts.append( text.trimmed() );
			}
		}
		break;
	default:
		setDefaultOptions( question );
		break;
	}

	return question;
}



void SurveyComposeDialog::accept()
{
	const auto newQuestion = question();
	if( newQuestion.text.isEmpty() &&
		( newQuestion.type == Type::Number || newQuestion.type == Type::ShortText ) &&
		QMessageBox::question( this, windowTitle(), tr( "The question is empty. Send it anyway?" ) ) != QMessageBox::Yes )
	{
		return;
	}

	s_lastQuestion = newQuestion;
	s_hasLastQuestion = true;
	QDialog::accept();
}



void SurveyComposeDialog::setDefaultOptions( SurveyQuestion& question, int count )
{
	switch( question.type )
	{
	case Type::TrueFalse:
		question.options = QStringList{ tr( "True" ), tr( "False" ) };
		break;
	case Type::YesNo:
		question.options = QStringList{ tr( "Yes" ), tr( "No" ) };
		if( count >= 3 )
		{
			question.options.append( tr( "Not sure" ) );
		}
		break;
	case Type::SingleChoice:
	case Type::MultipleChoice:
		question.options.clear();
		for( int i = 0; i < ( count > 0 ? count : 4 ); ++i )
		{
			question.options.append( SurveyQuestion::letter( i ) );
		}
		break;
	case Type::Scale:
		question.options.clear();
		for( int i = 1; i <= ( count > 1 ? count : 5 ); ++i )
		{
			question.options.append( QString::number( i ) );
		}
		break;
	case Type::Agreement:
		question.options = QStringList{ tr( "Strongly disagree" ), tr( "Disagree" ), tr( "Neutral" ),
										tr( "Agree" ), tr( "Strongly agree" ) };
		break;
	case Type::Stars:
		question.options = QStringList{ QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
										QStringLiteral("4"), QStringLiteral("5") };
		break;
	case Type::Understanding:
		question.options = QStringList{ tr( "Yes, I do" ), tr( "Not sure yet" ), tr( "Not yet" ) };
		break;
	default:
		break;
	}
}
