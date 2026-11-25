/*
 * SurveyResultsDialog.h - live results of a quick survey in the Teacher Console (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 *  Anonymous surveys: the answers are kept in memory per connection only so that students can change
 *  their answer. Nothing that links an answer to a computer or user is shown or written to disk.
 */

#pragma once

#include <QDateTime>
#include <QDialog>
#include <QElapsedTimer>
#include <QTimer>
#include <functional>

#include "ComputerControlInterface.h"
#include "SurveyQuestion.h"

class QLabel;
class QListWidget;
class QPushButton;
class QTabWidget;
class QTreeWidget;

class SurveyChartWidget : public QWidget
{
	Q_OBJECT
public:
	struct Row {
		QString label;
		int count{0};
		bool correct{false};
		QColor color;
	};

	explicit SurveyChartWidget( QWidget* parent );

	void setRows( const QList<Row>& rows, int total );

	QSize sizeHint() const override;

protected:
	void paintEvent( QPaintEvent* event ) override;

private:
	QList<Row> m_rows;
	int m_total{0};

};



class SurveyResultsDialog : public QDialog
{
	Q_OBJECT
public:
	using SendFunction = std::function<void(const ComputerControlInterfaceList&)>;

	SurveyResultsDialog( const SurveyQuestion& question, const ComputerControlInterfaceList& computers,
						 const SendFunction& sendQuestion, const SendFunction& stopSurvey, QWidget* parent );

	const QString& surveyId() const
	{
		return m_question.id;
	}

	bool isOpen() const
	{
		return m_open;
	}

	void addAnswer( const ComputerControlInterface::Pointer& computer, const QJsonValue& answer );

	// stops the survey on the student computers and saves the results
	void closeSurvey();

	void reject() override;

private:
	struct Entry {
		bool answered{false};
		QJsonValue answer;
		qint64 seconds{0};
	};

	struct Group {
		QString label;
		int count{0};
		bool correct{false};
	};

	void updateView();
	void updateTimer();
	void checkConnections();
	void remind();
	void exportCsv();
	void openFolder();
	void saveResults();

	QList<SurveyChartWidget::Row> chartRows() const;
	QString summaryText() const;
	QString csvText() const;
	QJsonObject resultsJson() const;
	QString computerName( int index ) const;
	QString userName( int index ) const;
	int answeredCount() const;

	SurveyQuestion m_question;
	ComputerControlInterfaceList m_computers;
	QList<Entry> m_entries;
	QList<bool> m_wasConnected;
	SendFunction m_sendQuestion;
	SendFunction m_stopSurvey;
	bool m_open{true};
	QDateTime m_startTime;
	QElapsedTimer m_elapsed;
	QTimer m_timer;
	QTimer m_connectionTimer;
	QString m_savedFile;

	QLabel* m_timeLabel;
	QLabel* m_summaryLabel;
	QTabWidget* m_tabs;
	SurveyChartWidget* m_chart;
	QTreeWidget* m_table{nullptr};
	QListWidget* m_pendingList{nullptr};
	QLabel* m_pendingLabel;
	QPushButton* m_remindButton;
	QPushButton* m_closeSurveyButton;
	QPushButton* m_folderButton;

};
