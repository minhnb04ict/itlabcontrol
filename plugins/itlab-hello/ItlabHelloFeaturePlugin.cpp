/*
 * ItlabHelloFeaturePlugin.cpp - plugin mẫu "Xin chào" của ITLabControl
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

// Đọc phần giải thích tổng quan ở đầu file ItlabHelloFeaturePlugin.h trước.

#include <QMessageBox>

#include "ItlabHelloFeaturePlugin.h"
#include "ComputerControlInterface.h"
#include "FeatureWorkerManager.h"
#include "PlatformCoreFunctions.h"
#include "PlatformUserFunctions.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"


// ─────────────────────────────────────────────────────────────────────────────
//  Hàm khởi tạo: khai báo tính năng "Xin chào"
// ─────────────────────────────────────────────────────────────────────────────
ItlabHelloFeaturePlugin::ItlabHelloFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_helloFeature( Feature(
		// Tên nội bộ (không dấu, không khoảng trắng)
		QStringLiteral("ItlabHello"),
		// Action        = nút bấm một lần (không phải chế độ bật/tắt như Lock)
		// AllComponents = tính năng chạy ở cả Master, Server và Worker
		Feature::Flag::Action | Feature::Flag::AllComponents,
		// UUID của TÍNH NĂNG: tạo một lần, không bao giờ đổi
		Feature::Uid( "d243154f-6ab6-4125-a515-434b065a2f02" ),
		// Tính năng cha: không có
		Feature::Uid(),
		// Tên hiện trên nút của Console
		tr( "Say hello" ),
		// Tên khi tính năng đang bật (chỉ dùng cho loại Mode): để trống
		{},
		// Chú thích khi rê chuột vào nút
		tr( "Send a greeting to the selected student computers. "
			"Students click OK to confirm they received it." ),
		// Icon nằm trong file itlab-hello.qrc
		QStringLiteral(":/itlab-hello/itlab-hello.png") ) ),
	m_features( { m_helloFeature } )
{
}



const FeatureList& ItlabHelloFeaturePlugin::featureList() const
{
	return m_features;
}



// ═════════════════════════════════════════════════════════════════════════════
//  PHÍA MASTER (Console giáo viên)
// ═════════════════════════════════════════════════════════════════════════════

// Giáo viên bấm nút "Xin chào" → Veyon gọi hàm này với danh sách máy đang chọn
bool ItlabHelloFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
											const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(master)

	// Mọi plugin đều nhận lời gọi này; chỉ xử lý nếu đúng tính năng của mình
	if( feature.uid() != m_helloFeature.uid() )
	{
		return false;
	}

	// Ở đây có thể mở hộp thoại cho giáo viên nhập nội dung (xem plugin textmessage).
	// Plugin mẫu dùng lời chào cố định cho đơn giản.
	return controlFeature( m_helloFeature.uid(), Operation::Start,
						   { { argToString(Argument::Text), tr( "Hello everyone! Click OK to confirm." ) } },
						   computerControlInterfaces );
}



// Gửi lệnh thật sự tới các máy. Tách riêng khỏi startFeature() để CLI và Web API
// cũng gọi được (không cần giao diện).
bool ItlabHelloFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
											  const QVariantMap& arguments,
											  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid != m_helloFeature.uid() || operation != Operation::Start )
	{
		return false;
	}

	// Mỗi lần chào có một mã riêng (RequestId), để ghép câu trả lời với đúng lần chào
	const auto requestId = QUuid::createUuid();

	// Đóng gói lệnh (tính năng + lệnh ShowHello + các tham số) và
	// gửi tới tất cả máy đang chọn                                   → BƯỚC 1
	// Lưu ý: FeatureMessage không cho phép "auto x = FeatureMessage{...}" (hàm sao chép
	// được khai báo explicit), nên tạo và gửi ngay trong một câu lệnh.
	sendFeatureMessage( FeatureMessage{ m_helloFeature.uid(), FeatureCommand::ShowHello }
							.addArgument( Argument::RequestId, requestId )
							.addArgument( Argument::Text, arguments.value( argToString(Argument::Text) ) ),
						computerControlInterfaces );

	return true;
}



// Máy học sinh trả lời về Console                                   ← BƯỚC 4
bool ItlabHelloFeaturePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
													const FeatureMessage& message )
{
	if( message.featureUid() != m_helloFeature.uid() ||
		message.command<FeatureCommand>() != FeatureCommand::HelloAcknowledged )
	{
		return false;
	}

	const auto computerName = computerControlInterface->computer().displayName();
	const auto userName = message.argument( Argument::UserName ).toString();

	// Hộp thông báo KHÔNG chặn (show() thay vì exec()), tự xoá khi đóng.
	// Plugin thật nên gom kết quả của cả phòng vào một bảng thay vì mỗi máy một hộp.
	auto box = new QMessageBox( QMessageBox::Information,
								tr( "Say hello" ),
								tr( "Computer %1 (user %2) has confirmed." ).arg( computerName, userName ) );
	box->setAttribute( Qt::WA_DeleteOnClose );
	box->show();

	return true;
}



// ═════════════════════════════════════════════════════════════════════════════
//  PHÍA SERVER (máy học sinh, chạy nền dưới dạng service)
// ═════════════════════════════════════════════════════════════════════════════

// Nhận lệnh từ Console                                              ← BƯỚC 1
bool ItlabHelloFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server,
													const MessageContext& messageContext,
													const FeatureMessage& message )
{
	if( message.featureUid() != m_helloFeature.uid() ||
		message.command<FeatureCommand>() != FeatureCommand::ShowHello )
	{
		return false;
	}

	// Server chạy nền, KHÔNG hiện được cửa sổ lên màn hình học sinh.
	// Vì vậy: ghi nhớ kết nối tới từ Console nào, rồi nhờ Worker hiện cửa sổ.
	m_pendingRequests[ message.argument( Argument::RequestId ).toUuid() ] = messageContext;

	// "Unmanaged session worker": Worker chạy trong phiên đăng nhập của học sinh,
	// tự khởi động nếu chưa chạy                                       → BƯỚC 2
	server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );

	return true;
}



// Nhận trả lời từ Worker                                            ← BƯỚC 3
bool ItlabHelloFeaturePlugin::handleFeatureMessageFromWorker( VeyonServerInterface& server,
															  const FeatureMessage& message )
{
	if( message.featureUid() != m_helloFeature.uid() ||
		message.command<FeatureCommand>() != FeatureCommand::HelloAcknowledged )
	{
		return false;
	}

	// Lấy (và xoá) ngữ cảnh đã ghi nhớ ở bước 1, rồi gửi trả lời về đúng Console đó
	const auto messageContext = m_pendingRequests.take( message.argument( Argument::RequestId ).toUuid() );

	// Nếu không tìm thấy RequestId, messageContext rỗng và hàm dưới không làm gì
	//                                                                   → BƯỚC 4
	return server.sendFeatureMessageReply( messageContext, message );
}



// ═════════════════════════════════════════════════════════════════════════════
//  PHÍA WORKER (máy học sinh, trong phiên đăng nhập của học sinh)
// ═════════════════════════════════════════════════════════════════════════════

// Nhận lệnh từ Server                                               ← BƯỚC 2
bool ItlabHelloFeaturePlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	if( message.featureUid() != m_helloFeature.uid() ||
		message.command<FeatureCommand>() != FeatureCommand::ShowHello )
	{
		return false;
	}

	const auto requestId = message.argument( Argument::RequestId ).toUuid();

	auto box = new QMessageBox( QMessageBox::Information,
								tr( "Greeting from the teacher" ),
								message.argument( Argument::Text ).toString(),
								QMessageBox::Ok );
	box->setAttribute( Qt::WA_DeleteOnClose );
	// ⚠ QUAN TRỌNG: mặc định Qt THOÁT chương trình khi cửa sổ cuối cùng đóng lại.
	// Nếu để mặc định, Worker thoát ngay khi học sinh bấm OK, trước khi tin nhắn
	// xác nhận kịp gửi đi qua mạng → Console không bao giờ nhận được (BƯỚC 4 mất).
	// Tắt thuộc tính này để Worker tiếp tục chạy nền; lần sau dùng lại luôn.
	box->setAttribute( Qt::WA_QuitOnClose, false );

	// Khi học sinh đóng hộp thoại (bấm OK) → gửi xác nhận về Server     → BƯỚC 3
	// Lambda [&worker, requestId]: đoạn mã chạy sau, "bắt" lại worker và requestId để dùng.
	connect( box, &QMessageBox::finished, this, [&worker, requestId]() {
		const auto userName = VeyonCore::platform().userFunctions().queryCurrentUserProperty(
			PlatformUserFunctions::UserProperty::LoginName );

		worker.sendFeatureMessageReply(
			FeatureMessage{ QUuid( "d243154f-6ab6-4125-a515-434b065a2f02" ), FeatureCommand::HelloAcknowledged }
				.addArgument( Argument::RequestId, requestId )
				.addArgument( Argument::UserName, userName ) );
	} );

	box->show();
	// Đưa cửa sổ lên trên cùng, kể cả khi học sinh đang dùng ứng dụng khác
	VeyonCore::platform().coreFunctions().raiseWindow( box, true );

	return true;
}
