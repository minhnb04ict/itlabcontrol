/*
 * SurveyQuestion.h - question of a quick survey (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 *  Answers are JSON values:
 *    - choice types (true/false, yes/no, single choice, scale, agreement, stars, understanding): index of the option
 *    - multiple choice: array of option indexes
 *    - number: number
 *    - short text: string
 */

#pragma once

#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

class SurveyQuestion
{
	Q_DECLARE_TR_FUNCTIONS(SurveyQuestion)
public:
	enum class Type {
		TrueFalse,
		YesNo,
		SingleChoice,
		MultipleChoice,
		Scale,
		Agreement,
		Stars,
		Understanding,
		Number,
		ShortText,
		Count
	};

	static constexpr int MaxTextLength = 200;
	static constexpr int MaxOptions = 10;

	QString id;
	Type type{Type::SingleChoice};
	QString text;
	QStringList options;		// labels of the answer buttons (all types except number and short text)
	QStringList optionTexts;	// single/multiple choice: content after the letter, may be empty
	QString lowLabel;			// scale: label of the lowest value
	QString highLabel;			// scale: label of the highest value
	int maxSelections{0};		// multiple choice: 0 = no limit
	QList<int> correctChoices;	// true/false, single and multiple choice
	bool hasCorrectNumber{false};
	double correctNumber{0};
	double tolerance{0};
	bool hasRange{false};
	double minimum{0};
	double maximum{0};
	QStringList correctTexts;	// short text: accepted answers
	int timeLimit{0};			// seconds, 0 = no limit
	bool allowChange{true};
	bool anonymous{false};

	QJsonObject toJson() const;
	static SurveyQuestion fromJson( const QJsonObject& object );

	// answered with one button (index of the option)
	bool isSingleIndex() const;
	// mean value makes sense (scale, agreement, stars)
	bool isRating() const;
	bool hasCorrectAnswer() const;

	static QString typeName( Type type );
	QString typeName() const
	{
		return typeName( type );
	}

	QString optionLabel( int index ) const;
	QString answerText( const QJsonValue& answer ) const;
	bool isValidAnswer( const QJsonValue& answer ) const;
	// -1: no correct answer defined, 0: wrong, 1: correct
	int correctness( const QJsonValue& answer ) const;

	static QString letter( int index );
	static QString normalizedText( const QString& text );
	static QString formatNumber( double value );

	static QString encodeAnswer( const QJsonValue& answer );
	static QJsonValue decodeAnswer( const QString& data );

};
