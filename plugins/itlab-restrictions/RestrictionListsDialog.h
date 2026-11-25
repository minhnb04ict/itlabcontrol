/*
 * RestrictionListsDialog.h - view and edit the website and application lists (ITLabControl)
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

class QLabel;
class QListWidget;

namespace Configuration { class Object; }

// Lets the teacher prepare the lists without applying any restriction. The lists are the same
// ones which are shown when a restriction is applied from the "Restrictions" menu.
class RestrictionListsDialog : public QDialog
{
	Q_OBJECT
public:
	RestrictionListsDialog( Configuration::Object* config, QWidget* parent );

	// configuration keys (group "ITLabRestrictions") of the per-teacher lists
	static QString configGroup();
	static QString allowedWebsitesKey();
	static QString blockedWebsitesKey();
	static QString allowedApplicationsKey();
	static QString blockedApplicationsKey();

	void accept() override;
	void reject() override;

private:
	struct ListTab
	{
		QString configKey;
		bool websites;
		bool allowList;
		QListWidget* list{nullptr};
		QLabel* countLabel{nullptr};
	};

	QWidget* createTab( ListTab& tab );
	QStringList normalized( const ListTab& tab, const QStringList& entries ) const;
	void addEntries( ListTab& tab );
	void editEntry( ListTab& tab );
	void removeEntries( ListTab& tab );
	void addPreset( ListTab& tab, const QStringList& entries );
	void setEntries( ListTab& tab, const QStringList& entries );
	QStringList entries( const ListTab& tab ) const;
	void updateCount( ListTab& tab );
	void save();

	Configuration::Object* m_config;
	QList<ListTab> m_tabs;
	bool m_modified{false};

};
