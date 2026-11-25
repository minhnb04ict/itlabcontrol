/*
 * WebHistoryDialog.h - show and export the web browsing history of student computers (ITLabControl)
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
#include <functional>

#include "ComputerControlInterface.h"

class QDateEdit;
class QLabel;
class QLineEdit;
class QTreeWidget;

class WebHistoryDialog : public QDialog
{
	Q_OBJECT
public:
	using RequestFunction = std::function<void(const ComputerControlInterfaceList&, qint64 fromMs, qint64 toMs)>;

	WebHistoryDialog( const ComputerControlInterfaceList& computers, const RequestFunction& request, QWidget* parent );

	// reply of a student computer
	void addEntries( ComputerControlInterface::Pointer computer, const QVariantList& entries );

private:
	void load();
	void applyFilter();
	void exportCsv();
	void updateStatus();

	ComputerControlInterfaceList m_computers;
	RequestFunction m_request;
	int m_answered{0};
	int m_requested{0};

	QDateEdit* m_fromEdit;
	QDateEdit* m_toEdit;
	QLineEdit* m_filterEdit;
	QTreeWidget* m_table;
	QLabel* m_statusLabel;

};
