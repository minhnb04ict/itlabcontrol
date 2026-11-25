/*
 * ItlabSurveyPlugin.h - quick surveys in the classroom (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 *  "Survey" button of the Teacher Console (phase 1b, docs/ITLabControl_Survey_Design.md):
 *    - new question (10 question types, correct answer, time limit, anonymous)
 *    - questions which are sent with one click (templates)
 *
 *  MASTER --(StartSurvey)--> SERVER --> WORKER (shows the question in the session of the student)
 *  WORKER --(Answer)--> SERVER --(reply)--> MASTER (live results)
 *  MASTER --(StopSurvey)--> SERVER --> WORKER (closes the window)
 */

#pragma once

#include <QPointer>

#include "Feature.h"
#include "FeatureProviderInterface.h"
#include "MessageContext.h"
#include "SurveyQuestion.h"

class SurveyAnswerWindow;
class SurveyResultsDialog;

class ItlabSurveyPlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "vn.itlabcontrol.Plugins.ItlabSurvey")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)

public:
	enum class FeatureCommand {
		StartSurvey,
		StopSurvey,
		Answer,
	};
	Q_ENUM(FeatureCommand)

	enum class Argument {
		Survey,
		SurveyId,
		Answer,
	};
	Q_ENUM(Argument)

	explicit ItlabSurveyPlugin( QObject* parent = nullptr );
	~ItlabSurveyPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("4d7e1b38-9c2a-4f65-a8d3-1e6b5c4a7f92") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("ItlabSurvey");
	}

	QString description() const override
	{
		return tr( "Quick surveys with live results" );
	}

	QString vendor() const override
	{
		return QStringLiteral("ITLabControl");
	}

	QString copyright() const override
	{
		return QStringLiteral("ITLabControl contributors");
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	// MASTER
	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;
	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						 const ComputerControlInterfaceList& computerControlInterfaces ) override
	{
		// surveys are always started from the Teacher Console dialogs
		Q_UNUSED(featureUid) Q_UNUSED(operation) Q_UNUSED(arguments) Q_UNUSED(computerControlInterfaces)
		return false;
	}
	bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
							   const FeatureMessage& message ) override;

	// SERVER
	bool handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
							   const FeatureMessage& message ) override;
	bool handleFeatureMessageFromWorker( VeyonServerInterface& server, const FeatureMessage& message ) override;

	// WORKER
	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

private:
	void startSurvey( VeyonMasterInterface& master, const SurveyQuestion& question,
					  const ComputerControlInterfaceList& computers );
	void showQuestion( VeyonWorkerInterface& worker, const SurveyQuestion& question );

	const Feature m_surveyFeature;
	const Feature m_composeFeature;
	const Feature m_understandingFeature;
	const Feature m_doneFeature;
	const Feature m_chooseAnswerFeature;
	const Feature m_trueFalseFeature;
	const Feature m_rateLessonFeature;
	const FeatureList m_features;

	// MASTER
	QPointer<SurveyResultsDialog> m_resultsDialog;

	// SERVER: connection of the Teacher Console which sent the survey
	MessageContext m_masterContext;

	// WORKER
	QPointer<SurveyAnswerWindow> m_answerWindow;

};
