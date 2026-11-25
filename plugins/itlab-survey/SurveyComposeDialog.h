/*
 * SurveyComposeDialog.h - compose a quick survey question in the Teacher Console (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QDialog>

#include "SurveyQuestion.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QStackedWidget;

class SurveyComposeDialog : public QDialog
{
	Q_OBJECT
public:
	SurveyComposeDialog( SurveyQuestion::Type type, int computerCount, QWidget* parent );

	SurveyQuestion question() const;

	// default answer buttons of the types without editable options
	static void setDefaultOptions( SurveyQuestion& question, int count = 0 );

	void accept() override;

private:
	void updatePage();
	void updateChoiceRows();
	QStringList agreementLabels() const;

	QPlainTextEdit* m_textEdit;
	QComboBox* m_typeCombo;
	QStackedWidget* m_pages;

	// true / false
	QComboBox* m_trueFalseCorrect{nullptr};
	// yes / no
	QCheckBox* m_notSureCheck{nullptr};
	// single / multiple choice
	QSpinBox* m_choiceCount{nullptr};
	QGridLayout* m_choiceGrid{nullptr};
	QList<QLineEdit*> m_choiceEdits;
	QList<QCheckBox*> m_choiceCorrect;
	QLabel* m_maxSelectionsLabel{nullptr};
	QSpinBox* m_maxSelections{nullptr};
	// scale
	QSpinBox* m_scaleCount{nullptr};
	QLineEdit* m_lowLabel{nullptr};
	QLineEdit* m_highLabel{nullptr};
	// agreement
	QList<QLineEdit*> m_agreementEdits;
	// number
	QCheckBox* m_rangeCheck{nullptr};
	QDoubleSpinBox* m_minimum{nullptr};
	QDoubleSpinBox* m_maximum{nullptr};
	QCheckBox* m_correctNumberCheck{nullptr};
	QDoubleSpinBox* m_correctNumber{nullptr};
	QDoubleSpinBox* m_tolerance{nullptr};
	// short text
	QLineEdit* m_correctTexts{nullptr};

	QComboBox* m_timeCombo;
	QCheckBox* m_allowChangeCheck;
	QCheckBox* m_anonymousCheck;

	static SurveyQuestion s_lastQuestion;
	static bool s_hasLastQuestion;

};
