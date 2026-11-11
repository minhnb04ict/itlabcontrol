/*
 * ItlabClearDesktopFeaturePlugin.h - dọn màn hình máy học sinh
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
 *  Tính năng "Dọn màn hình" (Clear desktop), đặc tả: docs/ITLabControl_Design_Plan.md mục 3.1
 *
 *  Nút trên Console có menu con:
 *    - Bấm thẳng vào nút / "Thu nhỏ tất cả cửa sổ": thu nhỏ mọi cửa sổ trên máy HS (không mất dữ liệu)
 *    - "Mở rộng tất cả cửa sổ": mở lại các cửa sổ vừa thu nhỏ về kích thước trước đó
 *    - "Đóng tất cả ứng dụng": đóng NGAY mọi ứng dụng đang mở (bài chưa lưu sẽ mất),
 *      có hộp xác nhận cho GV; luôn bỏ qua máy localhost (máy GV)
 *
 *  Đường đi (giống itlab-hello, nhưng một chiều):
 *    MASTER --(lệnh)--> SERVER --(chuyển tiếp)--> WORKER (chạy trong phiên của HS, xử lý cửa sổ)
 *
 *  Không bao giờ thu nhỏ/đóng: tiến trình hệ thống Windows, ITLabControl (veyon-*.exe) và danh sách
 *  ứng dụng được bảo vệ do IT cấu hình:
 *    veyon-cli config set ClearDesktop/ProtectedApplications "phanmemthi.exe;khac.exe"
 */

#pragma once

#include "Feature.h"
#include "FeatureProviderInterface.h"

class ItlabClearDesktopFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "vn.itlabcontrol.Plugins.ItlabClearDesktop")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)

public:
	explicit ItlabClearDesktopFeaturePlugin( QObject* parent = nullptr );
	~ItlabClearDesktopFeaturePlugin() override = default;

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("9dd91b45-e2e0-4fe6-8dbb-12bed5056392") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("ItlabClearDesktop");
	}

	QString description() const override
	{
		return tr( "Clear the desktop of student computers by minimizing or closing all applications" );
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

	// SERVER
	bool handleFeatureMessage( VeyonServerInterface& server,
							   const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	// WORKER
	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

private:
	enum class FeatureCommand {
		MinimizeAllWindows,
		CloseAllApplications,
		RestoreAllWindows   // thêm ở cuối để giữ nguyên số của các lệnh cũ
	};

	// tên file (chữ thường) của các tiến trình không bao giờ bị thu nhỏ/đóng
	static QStringList protectedProcessNames();

	static void minimizeAllWindows();
	static void restoreAllWindows();
	static void closeAllApplications();

	const Feature m_clearDesktopFeature;
	const Feature m_minimizeAllFeature;
	const Feature m_restoreAllFeature;
	const Feature m_closeAllFeature;
	const FeatureList m_features;

};
