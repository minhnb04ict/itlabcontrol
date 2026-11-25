/*
 * TeacherServiceControl.cpp - run the ITLabControl service only while the Teacher Console is open (ITLabControl)
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include <QElapsedTimer>
#include <QThread>

#include <veyonconfig.h>

#include "TeacherServiceControl.h"
#include "VeyonConfiguration.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif


bool TeacherServiceControl::isTeacherComputer()
{
	return VeyonCore::config().value( QStringLiteral("Role"), QStringLiteral("ITLab"), QString{} ).toString()
			.compare( QLatin1String("Teacher"), Qt::CaseInsensitive ) == 0;
}



#ifdef Q_OS_WIN

namespace {

SC_HANDLE openService( DWORD access )
{
	const auto manager = OpenSCManagerW( nullptr, nullptr, SC_MANAGER_CONNECT );
	if( manager == nullptr )
	{
		return nullptr;
	}
	const auto service = OpenServiceW( manager, QStringLiteral( ITLAB_SERVICE_NAME ).toStdWString().c_str(), access );
	CloseServiceHandle( manager );
	return service;
}

DWORD serviceState( SC_HANDLE service )
{
	SERVICE_STATUS status{};
	return QueryServiceStatus( service, &status ) ? status.dwCurrentState : 0;
}

}



void TeacherServiceControl::startService()
{
	const auto service = openService( SERVICE_START | SERVICE_QUERY_STATUS );
	if( service == nullptr )
	{
		vWarning() << "could not open the service" << GetLastError();
		return;
	}

	if( serviceState( service ) != SERVICE_RUNNING )
	{
		if( StartServiceW( service, 0, nullptr ) == FALSE && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING )
		{
			vWarning() << "could not start the service" << GetLastError();
		}
		else
		{
			// the Console connects to the local server, give the service a moment to start it
			QElapsedTimer timer;
			timer.start();
			while( serviceState( service ) != SERVICE_RUNNING && timer.elapsed() < 10000 )
			{
				QThread::msleep( 100 );
			}
		}
	}

	CloseServiceHandle( service );
}



void TeacherServiceControl::stopService()
{
	const auto service = openService( SERVICE_STOP | SERVICE_QUERY_STATUS );
	if( service == nullptr )
	{
		return;
	}

	if( serviceState( service ) == SERVICE_RUNNING )
	{
		SERVICE_STATUS status{};
		if( ControlService( service, SERVICE_CONTROL_STOP, &status ) == FALSE )
		{
			vWarning() << "could not stop the service" << GetLastError();
		}
	}

	CloseServiceHandle( service );
}

#else

void TeacherServiceControl::startService()
{
}

void TeacherServiceControl::stopService()
{
}

#endif
