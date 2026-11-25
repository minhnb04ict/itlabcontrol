/*
 * ItlabSurveyPlugin.cpp - quick surveys in the classroom (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <QCoreApplication>
#include <QJsonDocument>
#include <QMessageBox>
#include <QTimer>

#include "ComputerControlInterface.h"
#include "FeatureWorkerManager.h"
#include "ItlabSurveyPlugin.h"
#include "PlatformCoreFunctions.h"
#include "SurveyAnswerWindow.h"
#include "SurveyComposeDialog.h"
#include "SurveyResultsDialog.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"


ItlabSurveyPlugin::ItlabSurveyPlugin( QObject* parent ) :
	QObject( parent ),
	m_surveyFeature( QStringLiteral("ItlabSurvey"),
					 Feature::Flag::Action | Feature::Flag::AllComponents,
					 Feature::Uid( "6a2f8c41-5d7b-4e93-b0a6-2c9e8d7f1b53" ),
					 Feature::Uid(),
					 tr( "Survey" ), {},
					 tr( "Ask the class a quick question and see the answers immediately." ),
					 QStringLiteral(":/itlab-survey/itlab-survey.png") ),
	m_composeFeature( QStringLiteral("ItlabSurveyCompose"),
					  Feature::Flag::Action | Feature::Flag::AllComponents,
					  Feature::Uid( "7b3a9d52-6e8c-4fa4-81b7-3daf9e8a2c64" ),
					  m_surveyFeature.uid(),
					  tr( "New question..." ), {},
					  tr( "Write a question, choose the type of answers and send it to the students." ) ),
	m_understandingFeature( QStringLiteral("ItlabSurveyUnderstanding"),
							Feature::Flag::Action | Feature::Flag::AllComponents,
							Feature::Uid( "8c4bae63-7f9d-4ab5-92c8-4eb0af9b3d75" ),
							m_surveyFeature.uid(),
							SurveyComposeDialog::templateName( SurveyComposeDialog::Template::Understanding ), {}, {} ),
	m_doneFeature( QStringLiteral("ItlabSurveyDone"),
				   Feature::Flag::Action | Feature::Flag::AllComponents,
				   Feature::Uid( "9d5cbf74-80ae-4bc6-a3d9-5fc1b0ac4e86" ),
				   m_surveyFeature.uid(),
				   SurveyComposeDialog::templateName( SurveyComposeDialog::Template::Done ), {}, {} ),
	m_chooseAnswerFeature( QStringLiteral("ItlabSurveyChooseAnswer"),
						   Feature::Flag::Action | Feature::Flag::AllComponents,
						   Feature::Uid( "ae6dc085-91bf-4cd7-b4ea-60d2c1bd5f97" ),
						   m_surveyFeature.uid(),
						   SurveyComposeDialog::templateName( SurveyComposeDialog::Template::ChooseAnswer ), {}, {} ),
	m_trueFalseFeature( QStringLiteral("ItlabSurveyTrueFalse"),
						Feature::Flag::Action | Feature::Flag::AllComponents,
						Feature::Uid( "bf7ed196-a2c0-4de8-85fb-71e3d2ce6a08" ),
						m_surveyFeature.uid(),
						SurveyComposeDialog::templateName( SurveyComposeDialog::Template::TrueOrFalse ), {}, {} ),
	m_rateLessonFeature( QStringLiteral("ItlabSurveyRateLesson"),
						 Feature::Flag::Action | Feature::Flag::AllComponents,
						 Feature::Uid( "c08fe2a7-b3d1-4ef9-96ac-82f4e3df7b19" ),
						 m_surveyFeature.uid(),
						 SurveyComposeDialog::templateName( SurveyComposeDialog::Template::RateLesson ), {}, {} ),
	m_features( { m_surveyFeature, m_composeFeature, m_understandingFeature, m_doneFeature,
				  m_chooseAnswerFeature, m_trueFalseFeature, m_rateLessonFeature } )
{
}



// ═════════════════════════════════════════════════════════════════════════════
//  MASTER
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabSurveyPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
									  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( hasFeature( feature.uid() ) == false || feature == m_surveyFeature )
	{
		return false;
	}

	// the teacher computer does not answer its own questions
	auto computers = computerControlInterfaces;
	computers.removeLocalHostInterfaces();
	if( computers.isEmpty() )
	{
		QMessageBox::information( master.mainWindow(), tr( "Survey" ),
								  tr( "There are no student computers to send the question to." ) );
		return true;
	}

	SurveyQuestion question;
	if( feature == m_composeFeature )
	{
		SurveyComposeDialog dialog( computers.size(), master.mainWindow() );
		if( dialog.exec() != QDialog::Accepted )
		{
			return true;
		}
		question = dialog.question();
	}
	else
	{
		const QMap<Feature::Uid, SurveyComposeDialog::Template> templates{
			{ m_understandingFeature.uid(), SurveyComposeDialog::Template::Understanding },
			{ m_doneFeature.uid(), SurveyComposeDialog::Template::Done },
			{ m_chooseAnswerFeature.uid(), SurveyComposeDialog::Template::ChooseAnswer },
			{ m_trueFalseFeature.uid(), SurveyComposeDialog::Template::TrueOrFalse },
			{ m_rateLessonFeature.uid(), SurveyComposeDialog::Template::RateLesson },
		};
		if( templates.contains( feature.uid() ) == false )
		{
			return false;
		}
		question = SurveyComposeDialog::templateQuestion( templates[feature.uid()] );
	}

	startSurvey( master, question, computers );
	return true;
}



void ItlabSurveyPlugin::startSurvey( VeyonMasterInterface& master, const SurveyQuestion& question,
									 const ComputerControlInterfaceList& computers )
{
	// only one survey at a time
	if( m_resultsDialog )
	{
		if( m_resultsDialog->isOpen() &&
			QMessageBox::question( master.mainWindow(), tr( "Survey" ),
								   tr( "Another survey is still open. Close it and send the new question?" ) ) != QMessageBox::Yes )
		{
			m_resultsDialog->raise();
			m_resultsDialog->activateWindow();
			return;
		}
		m_resultsDialog->closeSurvey();
		m_resultsDialog->close();
	}

	const auto surveyData = QString::fromUtf8( QJsonDocument( question.toJson() ).toJson( QJsonDocument::Compact ) );
	const auto surveyId = question.id;

	auto sendQuestion = [this, surveyData]( const ComputerControlInterfaceList& targets ) {
		sendFeatureMessage( FeatureMessage{ m_surveyFeature.uid(), FeatureCommand::StartSurvey }
								.addArgument( Argument::Survey, surveyData ), targets );
	};
	auto stopSurvey = [this, surveyId]( const ComputerControlInterfaceList& targets ) {
		sendFeatureMessage( FeatureMessage{ m_surveyFeature.uid(), FeatureCommand::StopSurvey }
								.addArgument( Argument::SurveyId, surveyId ), targets );
	};

	m_resultsDialog = new SurveyResultsDialog( question, computers, sendQuestion, stopSurvey, master.mainWindow() );
	m_resultsDialog->show();

	sendQuestion( computers );
}



bool ItlabSurveyPlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
											  const FeatureMessage& message )
{
	if( message.featureUid() != m_surveyFeature.uid() ||
		message.command<FeatureCommand>() != FeatureCommand::Answer )
	{
		return false;
	}

	if( m_resultsDialog && m_resultsDialog->surveyId() == message.argument( Argument::SurveyId ).toString() )
	{
		m_resultsDialog->addAnswer( computerControlInterface,
									SurveyQuestion::decodeAnswer( message.argument( Argument::Answer ).toString() ) );
	}
	return true;
}



// ═════════════════════════════════════════════════════════════════════════════
//  SERVER: the window belongs to the session of the student, so the worker shows it
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabSurveyPlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
											  const FeatureMessage& message )
{
	if( message.featureUid() != m_surveyFeature.uid() )
	{
		return false;
	}

	switch( message.command<FeatureCommand>() )
	{
	case FeatureCommand::StartSurvey:
		m_masterContext = messageContext;
		server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );
		break;
	case FeatureCommand::StopSurvey:
		if( server.featureWorkerManager().isWorkerRunning( m_surveyFeature.uid() ) )
		{
			server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );
		}
		break;
	default:
		break;
	}

	return true;
}



bool ItlabSurveyPlugin::handleFeatureMessageFromWorker( VeyonServerInterface& server, const FeatureMessage& message )
{
	if( message.featureUid() != m_surveyFeature.uid() ||
		message.command<FeatureCommand>() != FeatureCommand::Answer )
	{
		return false;
	}

	// forward the answer to the Teacher Console which sent the survey
	server.sendFeatureMessageReply( m_masterContext, message );
	return true;
}



// ═════════════════════════════════════════════════════════════════════════════
//  WORKER
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabSurveyPlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	if( message.featureUid() != m_surveyFeature.uid() )
	{
		return false;
	}

	switch( message.command<FeatureCommand>() )
	{
	case FeatureCommand::StartSurvey:
	{
		const auto question = SurveyQuestion::fromJson(
			QJsonDocument::fromJson( message.argument( Argument::Survey ).toString().toUtf8() ).object() );
		if( question.id.isEmpty() == false )
		{
			showQuestion( worker, question );
		}
		break;
	}
	case FeatureCommand::StopSurvey:
	{
		const auto surveyId = message.argument( Argument::SurveyId ).toString();
		if( m_answerWindow )
		{
			if( surveyId.isEmpty() || m_answerWindow->surveyId() == surveyId )
			{
				m_answerWindow->finish();
			}
		}
		else
		{
			QTimer::singleShot( 500, QCoreApplication::instance(), &QCoreApplication::quit );
		}
		break;
	}
	default:
		break;
	}

	return true;
}



void ItlabSurveyPlugin::showQuestion( VeyonWorkerInterface& worker, const SurveyQuestion& question )
{
	// the question is already shown (e.g. sent again after a reconnect)
	if( m_answerWindow && m_answerWindow->surveyId() == question.id )
	{
		VeyonCore::platform().coreFunctions().raiseWindow( m_answerWindow, true );
		return;
	}

	if( m_answerWindow )
	{
		m_answerWindow->finish();
	}

	auto workerPointer = &worker;
	const auto surveyId = question.id;
	m_answerWindow = new SurveyAnswerWindow( question, [this, workerPointer, surveyId]( const QJsonValue& answer ) {
		workerPointer->sendFeatureMessageReply( FeatureMessage{ m_surveyFeature.uid(), FeatureCommand::Answer }
													.addArgument( Argument::SurveyId, surveyId )
													.addArgument( Argument::Answer, SurveyQuestion::encodeAnswer( answer ) ) );
	} );

	// the worker is only needed while a question is shown
	connect( m_answerWindow, &QObject::destroyed, this, [this]() {
		QTimer::singleShot( 500, this, [this]() {
			if( m_answerWindow.isNull() )
			{
				QCoreApplication::quit();
			}
		} );
	} );

	m_answerWindow->show();
	VeyonCore::platform().coreFunctions().raiseWindow( m_answerWindow, true );
}
