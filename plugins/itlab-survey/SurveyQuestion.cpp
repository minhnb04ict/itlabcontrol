/*
 * SurveyQuestion.cpp - question of a quick survey (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QRegularExpression>

#include "SurveyQuestion.h"


namespace {

QStringList toStringList( const QJsonValue& value )
{
	QStringList list;
	const auto array = value.toArray();
	for( const auto& item : array )
	{
		list.append( item.toString() );
	}
	return list;
}

}



QJsonObject SurveyQuestion::toJson() const
{
	QJsonArray correct;
	for( const auto choice : correctChoices )
	{
		correct.append( choice );
	}

	QJsonObject object;
	object[QStringLiteral("id")] = id;
	object[QStringLiteral("type")] = int( type );
	object[QStringLiteral("text")] = text;
	object[QStringLiteral("options")] = QJsonArray::fromStringList( options );
	object[QStringLiteral("optionTexts")] = QJsonArray::fromStringList( optionTexts );
	object[QStringLiteral("lowLabel")] = lowLabel;
	object[QStringLiteral("highLabel")] = highLabel;
	object[QStringLiteral("maxSelections")] = maxSelections;
	object[QStringLiteral("correctChoices")] = correct;
	object[QStringLiteral("hasCorrectNumber")] = hasCorrectNumber;
	object[QStringLiteral("correctNumber")] = correctNumber;
	object[QStringLiteral("tolerance")] = tolerance;
	object[QStringLiteral("hasRange")] = hasRange;
	object[QStringLiteral("minimum")] = minimum;
	object[QStringLiteral("maximum")] = maximum;
	object[QStringLiteral("correctTexts")] = QJsonArray::fromStringList( correctTexts );
	object[QStringLiteral("timeLimit")] = timeLimit;
	object[QStringLiteral("allowChange")] = allowChange;
	object[QStringLiteral("anonymous")] = anonymous;
	return object;
}



SurveyQuestion SurveyQuestion::fromJson( const QJsonObject& object )
{
	SurveyQuestion question;
	question.id = object.value( QStringLiteral("id") ).toString();
	question.type = Type( qBound( 0, object.value( QStringLiteral("type") ).toInt(), int(Type::Count) - 1 ) );
	question.text = object.value( QStringLiteral("text") ).toString();
	question.options = toStringList( object.value( QStringLiteral("options") ) ).mid( 0, MaxOptions );
	question.optionTexts = toStringList( object.value( QStringLiteral("optionTexts") ) ).mid( 0, MaxOptions );
	question.lowLabel = object.value( QStringLiteral("lowLabel") ).toString();
	question.highLabel = object.value( QStringLiteral("highLabel") ).toString();
	question.maxSelections = object.value( QStringLiteral("maxSelections") ).toInt();
	const auto correct = object.value( QStringLiteral("correctChoices") ).toArray();
	for( const auto& choice : correct )
	{
		question.correctChoices.append( choice.toInt() );
	}
	question.hasCorrectNumber = object.value( QStringLiteral("hasCorrectNumber") ).toBool();
	question.correctNumber = object.value( QStringLiteral("correctNumber") ).toDouble();
	question.tolerance = object.value( QStringLiteral("tolerance") ).toDouble();
	question.hasRange = object.value( QStringLiteral("hasRange") ).toBool();
	question.minimum = object.value( QStringLiteral("minimum") ).toDouble();
	question.maximum = object.value( QStringLiteral("maximum") ).toDouble();
	question.correctTexts = toStringList( object.value( QStringLiteral("correctTexts") ) );
	question.timeLimit = qMax( 0, object.value( QStringLiteral("timeLimit") ).toInt() );
	question.allowChange = object.value( QStringLiteral("allowChange") ).toBool( true );
	question.anonymous = object.value( QStringLiteral("anonymous") ).toBool();
	return question;
}



bool SurveyQuestion::isSingleIndex() const
{
	return type != Type::MultipleChoice && type != Type::Number && type != Type::ShortText;
}



bool SurveyQuestion::isRating() const
{
	return type == Type::Scale || type == Type::Agreement || type == Type::Stars;
}



bool SurveyQuestion::hasCorrectAnswer() const
{
	switch( type )
	{
	case Type::TrueFalse:
	case Type::SingleChoice:
	case Type::MultipleChoice:
		return correctChoices.isEmpty() == false;
	case Type::Number:
		return hasCorrectNumber;
	case Type::ShortText:
		return correctTexts.isEmpty() == false;
	default:
		break;
	}
	return false;
}



QString SurveyQuestion::typeName( Type type )
{
	switch( type )
	{
	case Type::TrueFalse: return tr( "True / false" );
	case Type::YesNo: return tr( "Yes / no" );
	case Type::SingleChoice: return tr( "Single choice (A, B, C, ...)" );
	case Type::MultipleChoice: return tr( "Multiple choice" );
	case Type::Scale: return tr( "Scale 1 ... n" );
	case Type::Agreement: return tr( "Agreement (5 levels)" );
	case Type::Stars: return tr( "Star rating (1 - 5)" );
	case Type::Understanding: return tr( "Three colored buttons" );
	case Type::Number: return tr( "Number" );
	case Type::ShortText: return tr( "Short text answer" );
	default: break;
	}
	return {};
}



QString SurveyQuestion::letter( int index )
{
	return QString( QChar( QLatin1Char('A').unicode() + index ) );
}



QString SurveyQuestion::optionLabel( int index ) const
{
	if( index < 0 || index >= options.size() )
	{
		return {};
	}
	const auto content = optionTexts.value( index ).trimmed();
	if( content.isEmpty() )
	{
		return options[index];
	}
	return options[index] + QStringLiteral(". ") + content;
}



QString SurveyQuestion::answerText( const QJsonValue& answer ) const
{
	if( isSingleIndex() )
	{
		return optionLabel( answer.toInt( -1 ) );
	}

	switch( type )
	{
	case Type::MultipleChoice:
	{
		QStringList letters;
		const auto array = answer.toArray();
		for( const auto& choice : array )
		{
			letters.append( options.value( choice.toInt( -1 ) ) );
		}
		return letters.isEmpty() ? tr( "(nothing selected)" ) : letters.join( QStringLiteral(", ") );
	}
	case Type::Number:
		return formatNumber( answer.toDouble() );
	case Type::ShortText:
		return answer.toString();
	default:
		break;
	}
	return {};
}



bool SurveyQuestion::isValidAnswer( const QJsonValue& answer ) const
{
	if( isSingleIndex() )
	{
		const auto index = answer.toInt( -1 );
		return answer.isDouble() && index >= 0 && index < options.size();
	}

	switch( type )
	{
	case Type::MultipleChoice:
	{
		if( answer.isArray() == false )
		{
			return false;
		}
		const auto array = answer.toArray();
		if( maxSelections > 0 && array.size() > maxSelections )
		{
			return false;
		}
		for( const auto& choice : array )
		{
			const auto index = choice.toInt( -1 );
			if( index < 0 || index >= options.size() )
			{
				return false;
			}
		}
		return true;
	}
	case Type::Number:
		return answer.isDouble() && ( hasRange == false || ( answer.toDouble() >= minimum && answer.toDouble() <= maximum ) );
	case Type::ShortText:
		return answer.isString() && answer.toString().trimmed().isEmpty() == false &&
				answer.toString().size() <= MaxTextLength;
	default:
		break;
	}
	return false;
}



int SurveyQuestion::correctness( const QJsonValue& answer ) const
{
	if( hasCorrectAnswer() == false )
	{
		return -1;
	}

	switch( type )
	{
	case Type::TrueFalse:
	case Type::SingleChoice:
		return correctChoices.contains( answer.toInt( -1 ) ) ? 1 : 0;
	case Type::MultipleChoice:
	{
		QList<int> selected;
		const auto array = answer.toArray();
		for( const auto& choice : array )
		{
			selected.append( choice.toInt() );
		}
		auto expected = correctChoices;
		std::sort( selected.begin(), selected.end() );
		std::sort( expected.begin(), expected.end() );
		selected.erase( std::unique( selected.begin(), selected.end() ), selected.end() );
		return selected == expected ? 1 : 0;
	}
	case Type::Number:
		return qAbs( answer.toDouble() - correctNumber ) <= qAbs( tolerance ) + 1e-9 ? 1 : 0;
	case Type::ShortText:
	{
		// ignore upper/lower case and spaces
		static const QRegularExpression spaces( QStringLiteral("\\s+") );
		auto given = normalizedText( answer.toString() );
		given.remove( spaces );
		for( const auto& correctText : correctTexts )
		{
			auto expected = normalizedText( correctText );
			expected.remove( spaces );
			if( expected.isEmpty() == false && given == expected )
			{
				return 1;
			}
		}
		return 0;
	}
	default:
		break;
	}
	return -1;
}



QString SurveyQuestion::normalizedText( const QString& text )
{
	return text.simplified().toLower();
}



QString SurveyQuestion::formatNumber( double value )
{
	return QLocale().toString( value, 'g', 12 );
}



QString SurveyQuestion::encodeAnswer( const QJsonValue& answer )
{
	return QString::fromUtf8( QJsonDocument( QJsonArray{ answer } ).toJson( QJsonDocument::Compact ) );
}



QJsonValue SurveyQuestion::decodeAnswer( const QString& data )
{
	return QJsonDocument::fromJson( data.toUtf8() ).array().at( 0 );
}
