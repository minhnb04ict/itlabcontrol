/*
 * ItlabClearDesktopFeaturePlugin.cpp - dọn màn hình máy học sinh
 *
 * Copyright (c) 2026 ITLabControl contributors
 *
 * This file is part of ITLabControl, based on Veyon - https://veyon.io
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
 */

#include <windows.h>

#include <vector>

#include <QFileInfo>
#include <QMessageBox>
#include <QSet>

#include "ItlabClearDesktopFeaturePlugin.h"
#include "ComputerControlInterface.h"
#include "FeatureWorkerManager.h"
#include "VeyonConfiguration.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"


ItlabClearDesktopFeaturePlugin::ItlabClearDesktopFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	// Tính năng cha: nút "Dọn màn hình" trên thanh công cụ (bấm thẳng = thu nhỏ tất cả)
	m_clearDesktopFeature( QStringLiteral("ItlabClearDesktop"),
						   Feature::Flag::Action | Feature::Flag::AllComponents,
						   Feature::Uid( "a9baa88a-d76b-4b4a-a6b6-82609cc6f628" ),
						   Feature::Uid(),
						   tr( "Clear desktop" ), {},
						   tr( "Click this button to minimize all windows on the selected computers. "
							   "Use the menu of this button to close all applications instead." ),
						   QStringLiteral(":/itlab-cleardesktop/itlab-cleardesktop.png") ),
	// Hai tính năng con, hiện trong menu của nút (parentUid = UUID của tính năng cha)
	m_minimizeAllFeature( QStringLiteral("ItlabClearDesktopMinimizeAll"),
						  Feature::Flag::Action | Feature::Flag::AllComponents,
						  Feature::Uid( "4adf79ac-b24e-4032-b7b0-2d5f306e88e8" ),
						  m_clearDesktopFeature.uid(),
						  tr( "Minimize all windows" ), {}, {} ),
	m_restoreAllFeature( QStringLiteral("ItlabClearDesktopRestoreAll"),
						 Feature::Flag::Action | Feature::Flag::AllComponents,
						 Feature::Uid( "e1fad23a-cd6b-4517-a5e2-872901674876" ),
						 m_clearDesktopFeature.uid(),
						 tr( "Restore all windows" ), {}, {} ),
	m_closeAllFeature( QStringLiteral("ItlabClearDesktopCloseAll"),
					   Feature::Flag::Action | Feature::Flag::AllComponents,
					   Feature::Uid( "5b870b3f-0b5c-46e9-92e4-80cecf7afe9e" ),
					   m_clearDesktopFeature.uid(),
					   tr( "Close all applications" ), {}, {} ),
	// thứ tự trong menu: Thu nhỏ, Mở rộng, Đóng
	m_features( { m_clearDesktopFeature, m_minimizeAllFeature, m_restoreAllFeature, m_closeAllFeature } )
{
}



const FeatureList& ItlabClearDesktopFeaturePlugin::featureList() const
{
	return m_features;
}



// ═════════════════════════════════════════════════════════════════════════════
//  MASTER (Bảng điều khiển giáo viên)
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabClearDesktopFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
												   const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature == m_clearDesktopFeature || feature == m_minimizeAllFeature )
	{
		return controlFeature( m_minimizeAllFeature.uid(), Operation::Start, {}, computerControlInterfaces );
	}

	if( feature == m_restoreAllFeature )
	{
		return controlFeature( m_restoreAllFeature.uid(), Operation::Start, {}, computerControlInterfaces );
	}

	if( feature == m_closeAllFeature )
	{
		// Đóng ứng dụng làm mất bài chưa lưu nên LUÔN hỏi lại giáo viên
		if( QMessageBox::warning( master.mainWindow(), tr( "Close all applications" ),
								  tr( "All applications on the selected computers will be closed immediately. "
									  "Unsaved work of the students will be lost!<br><br>"
									  "Do you really want to continue?" ),
								  QMessageBox::Yes | QMessageBox::No, QMessageBox::No ) != QMessageBox::Yes )
		{
			return true;
		}

		auto targets = computerControlInterfaces;
		targets.removeLocalHostInterfaces();
		if( targets.size() < computerControlInterfaces.size() )
		{
			QMessageBox::information( master.mainWindow(), tr( "Close all applications" ),
									  tr( "Applications are never closed on this computer (the teacher computer)." ) );
		}

		return controlFeature( m_closeAllFeature.uid(), Operation::Start, {}, targets );
	}

	return false;
}



bool ItlabClearDesktopFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
													 const QVariantMap& arguments,
													 const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(arguments)

	if( operation != Operation::Start )
	{
		return false;
	}

	if( featureUid == m_clearDesktopFeature.uid() || featureUid == m_minimizeAllFeature.uid() )
	{
		sendFeatureMessage( FeatureMessage{ m_clearDesktopFeature.uid(), FeatureCommand::MinimizeAllWindows },
							computerControlInterfaces );
		return true;
	}

	if( featureUid == m_restoreAllFeature.uid() )
	{
		sendFeatureMessage( FeatureMessage{ m_clearDesktopFeature.uid(), FeatureCommand::RestoreAllWindows },
							computerControlInterfaces );
		return true;
	}

	if( featureUid == m_closeAllFeature.uid() )
	{
		// an toàn kể cả khi gọi từ CLI/Web API: không bao giờ đóng ứng dụng trên máy localhost
		auto targets = computerControlInterfaces;
		targets.removeLocalHostInterfaces();
		sendFeatureMessage( FeatureMessage{ m_clearDesktopFeature.uid(), FeatureCommand::CloseAllApplications },
							targets );
		return true;
	}

	return false;
}



// ═════════════════════════════════════════════════════════════════════════════
//  SERVER (máy học sinh, chạy nền): chuyển lệnh cho Worker trong phiên của học sinh
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabClearDesktopFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server,
														   const MessageContext& messageContext,
														   const FeatureMessage& message )
{
	Q_UNUSED(messageContext)

	if( message.featureUid() != m_clearDesktopFeature.uid() )
	{
		return false;
	}

	server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );

	return true;
}



// ═════════════════════════════════════════════════════════════════════════════
//  WORKER (máy học sinh, trong phiên đăng nhập của học sinh)
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabClearDesktopFeaturePlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	Q_UNUSED(worker)

	if( message.featureUid() != m_clearDesktopFeature.uid() )
	{
		return false;
	}

	switch( message.command<FeatureCommand>() )
	{
	case FeatureCommand::MinimizeAllWindows:
		minimizeAllWindows();
		return true;
	case FeatureCommand::CloseAllApplications:
		closeAllApplications();
		return true;
	case FeatureCommand::RestoreAllWindows:
		restoreAllWindows();
		return true;
	}

	return false;
}



// ─────────────────────────────────────────────────────────────────────────────
//  Xử lý cửa sổ bằng Win32 API
// ─────────────────────────────────────────────────────────────────────────────

namespace {

struct AppWindow
{
	HWND handle;
	DWORD processId;
	QString processName; // tên file, chữ thường, ví dụ "winword.exe"
	QString className;
};


QString processNameOf( DWORD processId )
{
	QString name;
	const auto process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId );
	if( process )
	{
		wchar_t path[MAX_PATH]{};
		DWORD size = MAX_PATH;
		if( QueryFullProcessImageNameW( process, 0, path, &size ) )
		{
			name = QFileInfo( QString::fromWCharArray( path, int(size) ) ).fileName().toLower();
		}
		CloseHandle( process );
	}
	return name;
}


BOOL CALLBACK collectWindow( HWND hwnd, LPARAM param )
{
	// chỉ lấy cửa sổ ứng dụng thật: đang hiện, không có cửa sổ chủ, không phải tool window, có tiêu đề
	if( IsWindowVisible( hwnd ) == FALSE ||
		GetWindow( hwnd, GW_OWNER ) != nullptr ||
		( GetWindowLongPtrW( hwnd, GWL_EXSTYLE ) & WS_EX_TOOLWINDOW ) ||
		GetWindowTextLengthW( hwnd ) == 0 )
	{
		return TRUE;
	}

	wchar_t className[256]{};
	GetClassNameW( hwnd, className, 256 );

	DWORD processId = 0;
	GetWindowThreadProcessId( hwnd, &processId );

	reinterpret_cast<std::vector<AppWindow>*>( param )->push_back(
		{ hwnd, processId, processNameOf( processId ), QString::fromWCharArray( className ) } );

	return TRUE;
}


std::vector<AppWindow> applicationWindows( const QStringList& protectedNames )
{
	std::vector<AppWindow> all;
	EnumWindows( collectWindow, reinterpret_cast<LPARAM>( &all ) );

	// màn hình nền, thanh tác vụ… của Windows: không bao giờ động tới
	static const QStringList shellClasses{
		QStringLiteral("Progman"), QStringLiteral("WorkerW"),
		QStringLiteral("Shell_TrayWnd"), QStringLiteral("Shell_SecondaryTrayWnd") };

	std::vector<AppWindow> result;
	for( const auto& window : all )
	{
		if( window.processId == GetCurrentProcessId() ||
			window.processName.isEmpty() ||
			window.processName.startsWith( QLatin1String("veyon-") ) ||
			protectedNames.contains( window.processName ) ||
			shellClasses.contains( window.className ) )
		{
			continue;
		}
		result.push_back( window );
	}

	return result;
}

}



QStringList ItlabClearDesktopFeaturePlugin::protectedProcessNames()
{
	// tiến trình của Windows không được đóng (explorer.exe được xử lý riêng: chỉ đóng cửa sổ thư mục)
	QStringList names{
		QStringLiteral("dwm.exe"), QStringLiteral("winlogon.exe"), QStringLiteral("csrss.exe"),
		QStringLiteral("sihost.exe"), QStringLiteral("ctfmon.exe"), QStringLiteral("lockapp.exe"),
		QStringLiteral("shellexperiencehost.exe"), QStringLiteral("startmenuexperiencehost.exe"),
		QStringLiteral("searchhost.exe"), QStringLiteral("searchapp.exe"), QStringLiteral("textinputhost.exe"),
		QStringLiteral("taskmgr.exe") };

	// danh sách do IT cấu hình, ví dụ phần mềm thi:
	//   veyon-cli config set ClearDesktop/ProtectedApplications "phanmemthi.exe;khac.exe"
	const auto configured = VeyonCore::config().value( QStringLiteral("ProtectedApplications"),
													   QStringLiteral("ClearDesktop"), QString() ).toString();
	for( const auto& name : configured.split( QLatin1Char(';'), Qt::SkipEmptyParts ) )
	{
		names.append( name.trimmed().toLower() );
	}

	return names;
}



// cửa sổ đã thu nhỏ ở lần "Thu nhỏ tất cả" gần nhất (Worker chạy liên tục nên còn nhớ được)
static std::vector<HWND> s_lastMinimizedWindows;



void ItlabClearDesktopFeaturePlugin::minimizeAllWindows()
{
	const auto windows = applicationWindows( protectedProcessNames() );
	s_lastMinimizedWindows.clear();
	for( const auto& window : windows )
	{
		// chỉ nhớ cửa sổ đang mở, để "Mở rộng" không mở lại cửa sổ học sinh đã tự thu nhỏ từ trước
		if( IsIconic( window.handle ) == FALSE )
		{
			s_lastMinimizedWindows.push_back( window.handle );
		}
		ShowWindowAsync( window.handle, SW_MINIMIZE );
	}

	vDebug() << "minimized" << windows.size() << "windows";
}



void ItlabClearDesktopFeaturePlugin::restoreAllWindows()
{
	// Mở lại đúng các cửa sổ vừa bị thu nhỏ, về kích thước trước đó (không phóng to toàn màn hình).
	// Nếu không còn danh sách (Worker đã khởi động lại), mở lại mọi cửa sổ ứng dụng đang thu nhỏ.
	int restored = 0;
	if( s_lastMinimizedWindows.empty() == false )
	{
		for( const auto handle : s_lastMinimizedWindows )
		{
			if( IsWindow( handle ) && IsIconic( handle ) )
			{
				ShowWindowAsync( handle, SW_RESTORE );
				++restored;
			}
		}
		s_lastMinimizedWindows.clear();
	}
	else
	{
		const auto windows = applicationWindows( protectedProcessNames() );
		for( const auto& window : windows )
		{
			if( IsIconic( window.handle ) )
			{
				ShowWindowAsync( window.handle, SW_RESTORE );
				++restored;
			}
		}
	}

	vDebug() << "restored" << restored << "windows";
}



void ItlabClearDesktopFeaturePlugin::closeAllApplications()
{
	const auto windows = applicationWindows( protectedProcessNames() );

	QSet<DWORD> terminated;
	for( const auto& window : windows )
	{
		// explorer.exe vừa là thanh tác vụ vừa là cửa sổ thư mục: chỉ đóng cửa sổ, không kết thúc tiến trình
		if( window.processName == QLatin1String("explorer.exe") )
		{
			PostMessageW( window.handle, WM_CLOSE, 0, 0 );
			continue;
		}

		if( terminated.contains( window.processId ) )
		{
			continue;
		}

		const auto process = OpenProcess( PROCESS_TERMINATE, FALSE, window.processId );
		if( process )
		{
			TerminateProcess( process, 0 );
			CloseHandle( process );
			terminated.insert( window.processId );
		}
		else
		{
			vWarning() << "could not terminate" << window.processName;
		}
	}

	vDebug() << "terminated" << terminated.size() << "processes";
}
