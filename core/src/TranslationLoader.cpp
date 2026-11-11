/*
 * TranslationLoader.cpp - implementation of TranslationLoader class
 *
 * Copyright (c) 2020-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QCoreApplication>
#include <QRegularExpression>
#include <QSettings>
#include <QTranslator>

#include "TranslationLoader.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"


TranslationLoader::TranslationLoader( const QString& resourceName )
{
	load( resourceName );
}



// ITLabControl: language codes are stored either as "<name> (<code>)" (Configurator) or as plain "<code>"
static QString languageCodeFromSetting( const QString& value )
{
	static const QRegularExpression configuredLocaleRX{QStringLiteral( "[^(]*\\(([^)]*)\\)")};
	const auto match = configuredLocaleRX.match( value );
	return match.hasMatch() ? match.captured( 1 ) : value.trimmed();
}



QString TranslationLoader::effectiveLanguageCode()
{
	// 1. per-user choice (e.g. language menu of the Teacher Console), no admin rights required
	const auto userLanguageCode = languageCodeFromSetting( userLanguage() );
	if( userLanguageCode.isEmpty() == false )
	{
		return userLanguageCode;
	}

	// 2. system-wide setting (Configurator)
	const auto systemLanguage = languageCodeFromSetting( VeyonCore::config().uiLanguage() );
	if( systemLanguage.isEmpty() == false )
	{
		return systemLanguage;
	}

	// 3. ITLabControl default: Vietnamese, regardless of the Windows display language
	return QStringLiteral("vi");
}



QString TranslationLoader::userLanguage()
{
	return QSettings( QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName() )
		.value( QStringLiteral("UI/Language") ).toString();
}



void TranslationLoader::setUserLanguage( const QString& languageCode )
{
	QSettings( QSettings::UserScope, QCoreApplication::organizationName(), QCoreApplication::applicationName() )
		.setValue( QStringLiteral("UI/Language"), languageCode );
}



bool TranslationLoader::load( const QString& resourceName )
{
	QLocale configuredLocale( effectiveLanguageCode() );

	if( configuredLocale.language() != QLocale::English &&
		VeyonCore::instance()->findChild<QTranslator *>( resourceName ) == nullptr )
	{
		const auto translationsDirectory = resourceName.startsWith( QLatin1String("qt") )
											  ? VeyonCore::qtTranslationsDirectory()
											  : VeyonCore::translationsDirectory();

		auto translator = new QTranslator( VeyonCore::instance() );
		translator->setObjectName( resourceName );

		if( configuredLocale == QLocale::C ||
			translator->load( QStringLiteral( "%1_%2.qm" ).arg( resourceName, configuredLocale.name() ),
							  translationsDirectory ) == false )
		{
			configuredLocale = QLocale::system(); // Flawfinder: ignore

			if( translator->load( QStringLiteral( "%1_%2.qm" ).arg( resourceName, configuredLocale.name() ),
								  translationsDirectory ) == false )
			{
				delete translator;
				return false;
			}
		}

		QLocale::setDefault( configuredLocale );

		QCoreApplication::installTranslator( translator );
	}

	return true;
}
