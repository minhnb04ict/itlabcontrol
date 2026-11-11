/*
 * ItlabRoomsPlugin.cpp - quản lý nhiều phòng máy
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

#include <QAbstractSocket>
#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>

#include "ItlabRoomsPlugin.h"
#include "PlatformUserFunctions.h"
#include "RoomConfigurationPage.h"
#include "RoomManagerDialog.h"
#include "VeyonConfiguration.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"


ItlabRoomsPlugin::ItlabRoomsPlugin( QObject* parent ) :
	QObject( parent ),
	// Master: nút trên thanh công cụ; Service: máy HS/GV trả lời truy vấn, nhận lệnh đổi phòng
	m_roomManagerFeature( QStringLiteral("ItlabRoomManager"),
						  Feature::Flag::Action | Feature::Flag::Master | Feature::Flag::Service,
						  Feature::Uid( "66f3ea93-0a5d-4a5f-b8a8-57b725a42c21" ),
						  Feature::Uid(),
						  tr( "Rooms" ), {},
						  tr( "Manage rooms and computers: add computers by name or name pattern, "
							  "discover computers on the network and assign rooms." ),
						  QStringLiteral(":/itlab-rooms/itlab-rooms.png") ),
	m_features( { m_roomManagerFeature } )
{
}



const FeatureList& ItlabRoomsPlugin::featureList() const
{
	return m_features;
}



// ═════════════════════════════════════════════════════════════════════════════
//  MASTER
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabRoomsPlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
									 const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(computerControlInterfaces)

	if( feature != m_roomManagerFeature )
	{
		return false;
	}

	if( m_dialog.isNull() )
	{
		m_dialog = new RoomManagerDialog( master, this );
		m_dialog->setAttribute( Qt::WA_DeleteOnClose );
	}

	m_dialog->show();
	m_dialog->raise();
	m_dialog->activateWindow();

	return true;
}



bool ItlabRoomsPlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
									   const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(featureUid)
	Q_UNUSED(operation)
	Q_UNUSED(arguments)
	Q_UNUSED(computerControlInterfaces)

	return false;
}



bool ItlabRoomsPlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
											 const FeatureMessage& message )
{
	if( message.featureUid() != m_roomManagerFeature.uid() )
	{
		return false;
	}

	// câu trả lời từ máy HS/GV: chuyển cho cửa sổ Quản lý phòng (nếu đang mở)
	if( m_dialog )
	{
		m_dialog->handleReply( computerControlInterface, message );
	}

	return true;
}



// ═════════════════════════════════════════════════════════════════════════════
//  SERVER
// ═════════════════════════════════════════════════════════════════════════════

bool ItlabRoomsPlugin::handleFeatureMessage( VeyonServerInterface& server,
											 const MessageContext& messageContext,
											 const FeatureMessage& message )
{
	if( message.featureUid() != m_roomManagerFeature.uid() )
	{
		return false;
	}

	switch( message.command<FeatureCommand>() )
	{
	case FeatureCommand::QueryInfo:
	{
		QStringList interfaces;
		const auto allInterfaces = QNetworkInterface::allInterfaces();
		for( const auto& networkInterface : allInterfaces )
		{
			if( networkInterface.flags().testFlag( QNetworkInterface::IsUp ) == false ||
				networkInterface.flags().testFlag( QNetworkInterface::IsLoopBack ) )
			{
				continue;
			}
			const auto entries = networkInterface.addressEntries();
			for( const auto& entry : entries )
			{
				if( entry.ip().protocol() == QAbstractSocket::IPv4Protocol )
				{
					interfaces.append( QStringLiteral("%1=%2").arg( entry.ip().toString(),
																	networkInterface.hardwareAddress() ) );
				}
			}
		}

		return server.sendFeatureMessageReply( messageContext,
			FeatureMessage{ m_roomManagerFeature.uid(), FeatureCommand::InfoReply }
				.addArgument( Argument::HostName, QHostInfo::localHostName() )
				.addArgument( Argument::Room, configuredRoom() )
				.addArgument( Argument::UserLogin, VeyonCore::platform().userFunctions().queryCurrentUserProperty(
								  PlatformUserFunctions::UserProperty::LoginName ) )
				.addArgument( Argument::Interfaces, interfaces ) );
	}

	case FeatureCommand::SetRoom:
		setConfiguredRoom( message.argument( Argument::Room ).toString() );
		vInfo() << "room of this computer set to" << configuredRoom();
		return server.sendFeatureMessageReply( messageContext,
			FeatureMessage{ m_roomManagerFeature.uid(), FeatureCommand::SetRoomReply }
				.addArgument( Argument::Success, true )
				.addArgument( Argument::Room, configuredRoom() ) );

	case FeatureCommand::SetDirectory:
	{
		// chỉ chấp nhận từ Console chạy trên CHÍNH máy này, để máy khác không sửa được danh sách phòng
		const auto socket = qobject_cast<QAbstractSocket*>( messageContext.ioDevice() );
		const auto fromLocalHost = socket && socket->peerAddress().isLoopback();

		bool success = false;
		const auto json = QJsonDocument::fromJson( message.argument( Argument::Directory ).toString().toUtf8() );
		if( fromLocalHost && json.isArray() )
		{
			VeyonCore::config().reloadFromStore();
			VeyonCore::config().setValue( QStringLiteral("NetworkObjects"), json.array(), QStringLiteral("BuiltinDirectory") );
			VeyonCore::config().flushStore();
			success = true;
			vInfo() << "network object directory updated with" << json.array().size() << "objects";
		}
		else
		{
			vWarning() << "rejected directory update (from localhost:" << fromLocalHost << ")";
		}

		return server.sendFeatureMessageReply( messageContext,
			FeatureMessage{ m_roomManagerFeature.uid(), FeatureCommand::SetDirectoryReply }
				.addArgument( Argument::Success, success ) );
	}

	default:
		break;
	}

	return false;
}



// ═════════════════════════════════════════════════════════════════════════════
//  CONFIGURATOR
// ═════════════════════════════════════════════════════════════════════════════

ConfigurationPage* ItlabRoomsPlugin::createConfigurationPage()
{
	return new RoomConfigurationPage();
}



// ═════════════════════════════════════════════════════════════════════════════
//  Dùng chung
// ═════════════════════════════════════════════════════════════════════════════

QString ItlabRoomsPlugin::configuredRoom()
{
	return VeyonCore::config().value( QStringLiteral("Room"), QStringLiteral("ITLab"), QString() ).toString();
}



void ItlabRoomsPlugin::setConfiguredRoom( const QString& room )
{
	VeyonCore::config().reloadFromStore();
	VeyonCore::config().setValue( QStringLiteral("Room"), room.simplified(), QStringLiteral("ITLab") );
	VeyonCore::config().flushStore();
}



QString ItlabRoomsPlugin::normalizedRoomName( const QString& room )
{
	// "Phòng  Thử Nghiệm" -> "phong thu nghiem"
	const auto decomposed = room.normalized( QString::NormalizationForm_D );
	QString result;
	result.reserve( decomposed.size() );
	for( const auto& c : decomposed )
	{
		if( c.category() == QChar::Mark_NonSpacing )
		{
			continue;
		}
		if( c == QChar(0x0111) || c == QChar(0x0110) ) // đ, Đ
		{
			result.append( QLatin1Char('d') );
			continue;
		}
		result.append( c );
	}

	return result.toLower().simplified();
}
