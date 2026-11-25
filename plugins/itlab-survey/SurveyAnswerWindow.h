/*
 * SurveyAnswerWindow.h - window on the student computer to answer a quick survey (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>
#include <functional>

#include "SurveyQuestion.h"

class QAbstractButton;
class QLabel;
class QLineEdit;
class QPushButton;

class SurveyAnswerWindow : public QWidget
{
	Q_OBJECT
public:
	using SubmitFunction = std::function<void(const QJsonValue& answer)>;

	SurveyAnswerWindow( const SurveyQuestion& question, const SubmitFunction& submit );

	const QString& surveyId() const
	{
		return m_question.id;
	}

	// the teacher closed the survey
	void finish();

protected:
	void closeEvent( QCloseEvent* event ) override;

private:
	void buildAnswerArea( QLayout* layout );
	void submit( const QJsonValue& answer );
	void showAnswerArea();
	void showSentArea();
	void updateCountdown();
	void submitMultipleChoice();
	void submitNumber();
	void submitText();

	SurveyQuestion m_question;
	SubmitFunction m_submitFunction;
	bool m_answered{false};
	bool m_allowClose{false};

	QElapsedTimer m_elapsed;
	QTimer m_countdownTimer;

	QWidget* m_answerArea{nullptr};
	QWidget* m_sentArea{nullptr};
	QLabel* m_countdownLabel{nullptr};
	QLabel* m_sentLabel{nullptr};
	QPushButton* m_changeButton{nullptr};
	QPushButton* m_submitButton{nullptr};
	QList<QAbstractButton*> m_choiceButtons;
	QLineEdit* m_numberEdit{nullptr};
	QLineEdit* m_textEdit{nullptr};

};
