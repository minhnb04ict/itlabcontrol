/*
 * ItlabHelloFeaturePlugin.h - plugin mẫu "Xin chào" của ITLabControl
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
 * ═══════════════════════════════════════════════════════════════════════════
 *  PLUGIN MẪU CHO MỌI TÍNH NĂNG itlab-*
 * ═══════════════════════════════════════════════════════════════════════════
 *
 *  Chức năng: giáo viên bấm nút "Xin chào" trên Console
 *             → máy học sinh hiện cửa sổ chào
 *             → học sinh bấm OK
 *             → Console hiện thông báo "Máy X đã xác nhận".
 *
 *  Một plugin Veyon được nạp vào CẢ 3 chương trình, mỗi chương trình gọi
 *  một nhóm hàm khác nhau:
 *
 *    ┌──────────────────┐   1. lệnh "Chào"    ┌──────────────────┐  2. chuyển tiếp  ┌──────────────────┐
 *    │ MASTER           │ ──────────────────► │ SERVER           │ ───────────────► │ WORKER           │
 *    │ (Console GV,     │                     │ (máy HS, chạy    │                  │ (máy HS, chạy    │
 *    │  máy giáo viên)  │                     │  nền, có quyền)  │                  │  trong phiên HS, │
 *    │                  │ ◄────────────────── │                  │ ◄─────────────── │  hiện cửa sổ)    │
 *    └──────────────────┘   4. "Đã xác nhận"  └──────────────────┘  3. HS bấm OK    └──────────────────┘
 *
 *    MASTER : startFeature(), controlFeature(), handleFeatureMessage(ComputerControlInterface…)
 *    SERVER : handleFeatureMessage(VeyonServerInterface…), handleFeatureMessageFromWorker()
 *    WORKER : handleFeatureMessage(VeyonWorkerInterface…)
 *
 *  Mọi dữ liệu trao đổi đều đóng gói trong FeatureMessage gồm:
 *    - featureUid : tính năng nào (UUID cố định của plugin)
 *    - command    : lệnh gì (enum FeatureCommand bên dưới)
 *    - arguments  : các tham số kèm theo (enum Argument bên dưới)
 */

#pragma once

#include <QMap>
#include <QUuid>

#include "Feature.h"
#include "FeatureProviderInterface.h"
#include "MessageContext.h"

// Một class plugin Veyon kế thừa 3 thứ:
//  - QObject                  : để dùng cơ chế của Qt (tr() dịch chuỗi, signal/slot…)
//  - FeatureProviderInterface : "plugin này cung cấp tính năng"
//  - PluginInterface          : thông tin chung của plugin (tên, phiên bản, UUID…)
class ItlabHelloFeaturePlugin : public QObject, FeatureProviderInterface, PluginInterface
{
	// Q_OBJECT: bắt buộc với mọi class kế thừa QObject (để công cụ moc của Qt sinh mã phụ)
	Q_OBJECT
	// Khai báo đây là plugin Qt, với mã định danh duy nhất
	Q_PLUGIN_METADATA(IID "vn.itlabcontrol.Plugins.ItlabHello")
	// Liệt kê các interface plugin này cài đặt, để Veyon nhận ra khi nạp
	Q_INTERFACES(PluginInterface FeatureProviderInterface)

public:
	// Các tham số gửi kèm FeatureMessage. Dùng enum thay vì chuỗi để tránh gõ sai.
	enum class Argument {
		RequestId,      // mã của một lần "chào", để ghép câu trả lời với đúng yêu cầu
		Text,           // nội dung lời chào
		UserName        // tên đăng nhập của học sinh đã bấm OK
	};
	Q_ENUM(Argument)

	explicit ItlabHelloFeaturePlugin( QObject* parent = nullptr );
	~ItlabHelloFeaturePlugin() override = default;

	// ───── Thông tin plugin (PluginInterface) ─────

	// UUID của PLUGIN: tạo một lần, không bao giờ đổi
	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("36e23968-3a0a-4392-a387-e027bcc54513") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber( 1, 0 );
	}

	QString name() const override
	{
		return QStringLiteral("ItlabHello");
	}

	QString description() const override
	{
		return tr( "Sample plugin: send a greeting to students and receive confirmations" );
	}

	QString vendor() const override
	{
		return QStringLiteral("ITLabControl");
	}

	QString copyright() const override
	{
		return QStringLiteral("ITLabControl contributors");
	}

	// ───── Danh sách tính năng (FeatureProviderInterface) ─────

	const FeatureList& featureList() const override;

	// ───── Phía MASTER (Console giáo viên) ─────

	// Được gọi khi giáo viên bấm nút trên thanh công cụ
	bool startFeature( VeyonMasterInterface& master, const Feature& feature,
					   const ComputerControlInterfaceList& computerControlInterfaces ) override;

	// Gửi lệnh tới các máy (dùng chung cho Console, CLI và Web API)
	bool controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
						 const ComputerControlInterfaceList& computerControlInterfaces ) override;

	// Nhận tin nhắn trả về từ một máy học sinh (bước 4)
	bool handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
							   const FeatureMessage& message ) override;

	// ───── Phía SERVER (máy học sinh, chạy nền) ─────

	// Nhận lệnh từ Console (bước 1) và chuyển cho Worker (bước 2)
	bool handleFeatureMessage( VeyonServerInterface& server,
							   const MessageContext& messageContext,
							   const FeatureMessage& message ) override;

	// Nhận trả lời từ Worker (bước 3) và gửi ngược về Console (bước 4)
	bool handleFeatureMessageFromWorker( VeyonServerInterface& server,
										 const FeatureMessage& message ) override;

	// ───── Phía WORKER (máy học sinh, trong phiên đăng nhập) ─────

	// Hiện cửa sổ chào; khi học sinh bấm OK thì gửi trả lời cho Server
	bool handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message ) override;

private:
	// Các lệnh của plugin
	enum class FeatureCommand {
		ShowHello,          // Console → máy HS: "hiện lời chào"
		HelloAcknowledged   // máy HS → Console: "học sinh đã bấm OK"
	};

	const Feature m_helloFeature;
	const FeatureList m_features;

	// Chỉ dùng ở SERVER: nhớ "ngữ cảnh kết nối" (kết nối nào tới từ Console nào)
	// theo từng RequestId, để khi Worker trả lời thì biết gửi về đâu.
	QMap<QUuid, MessageContext> m_pendingRequests;

};
