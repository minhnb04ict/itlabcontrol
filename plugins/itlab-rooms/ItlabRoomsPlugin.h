/*
 * ItlabRoomsPlugin.h - quản lý nhiều phòng máy
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

/*
 *  Quản lý nhiều phòng (đặc tả: docs/ITLabControl_Design_Plan.md, GĐ1 bước 2)
 *
 *  MASTER (Bảng điều khiển giáo viên):
 *    Nút "Quản lý phòng" mở RoomManagerDialog:
 *      - Phòng và máy: thêm/sửa/xoá phòng và máy, đặt phòng trên máy HS từ xa
 *      - Tạo theo mẫu tên: tiền tố + khoảng số (+ hậu tố)
 *      - Tự dò máy: quét mạng, chỉ máy cài ITLabControl (cùng khoá) mới trả lời
 *    Lưu: danh sách được gửi tới dịch vụ ITLabControl trên CHÍNH máy GV (localhost), dịch vụ chạy
 *    quyền SYSTEM ghi vào cấu hình. Giáo viên không cần quyền admin.
 *
 *  SERVER (máy HS và máy GV):
 *    - QueryInfo    -> InfoReply (tên máy, phòng đã khai báo, người đăng nhập, IP/MAC)
 *    - SetRoom      -> ghi tên phòng vào cấu hình máy (ITLab/Room)
 *    - SetDirectory -> ghi danh sách phòng/máy (chỉ chấp nhận từ localhost)
 *
 *  CONFIGURATOR: trang "Phòng máy" để sửa tên phòng của máy ngay trên máy đó.
 */

#pragma once

#include <QPointer>

#include "ConfigurationPagePluginInterface.h"
#include "Feature.h"
#include "FeatureProviderInterface.h"

class RoomManagerDialog;

class ItlabRoomsPlugin : public QObject, FeatureProviderInterface, PluginInterface, ConfigurationPagePluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "vn.itlabcontrol.Plugins.ItlabRooms")
	Q_INTERFACES(PluginInterface FeatureProviderInterface ConfigurationPagePluginInterface)

public:
	enum class Argument {
		HostName,
		Room,
		UserLogin,
		Interfaces,   // danh sách "IP=MAC"
		Success,
		Directory     // danh sách phòng/máy dạng JSON
	};
	Q_ENUM(Argument)

	enum class FeatureCommand {
		QueryInfo,
		InfoReply,
		SetRoom,
		SetRoomReply,
		SetDirectory,
		SetDirectoryReply
	};
	Q_ENUM(FeatureCommand)

	explicit ItlabRoomsPlugin( QObject* parent = nullptr );
	~ItlabRoomsPlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("263ba87a-059b-43c3-a50f-9e2010bd964e") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("ItlabRooms");
	}

	QString description() const override
	{
		return tr( "Manage rooms and computers, discover computers and assign rooms" );
	}

	QString vendor() const override
	{
		return QStringLiteral("ITLabControl");
	}

	QString copyright() const override
	{
		return QStringLiteral("ITLabControl contributors");
	}

	const FeatureList& featureList() const override;

	// MASTER
	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						 const ComputerControlInterfaceList& computerControlInterfaces ) override;

	bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
							   const FeatureMessage& message ) override;

	// SERVER
	bool handleFeatureMessage( VeyonServerInterface& server,
							   const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	// CONFIGURATOR
	ConfigurationPage* createConfigurationPage() override;

	// dùng chung cho dialog, trang cấu hình và server
	Feature::Uid featureUid() const
	{
		return m_roomManagerFeature.uid();
	}

	static QString configuredRoom();
	static void setConfiguredRoom( const QString& room );

	// so khớp tên phòng: không phân biệt hoa/thường, dấu tiếng Việt, khoảng trắng thừa
	static QString normalizedRoomName( const QString& room );

private:
	const Feature m_roomManagerFeature;
	const FeatureList m_features;

	QPointer<RoomManagerDialog> m_dialog;

};
