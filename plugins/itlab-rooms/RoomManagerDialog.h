/*
 * RoomManagerDialog.h - cửa sổ "Quản lý phòng" của Bảng điều khiển giáo viên
 *
 * Copyright (c) 2026 ITLabControl contributors
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
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

#include "ComputerControlInterface.h"
#include "FeatureMessage.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTcpSocket;
class QTreeWidget;
class QTreeWidgetItem;
class ItlabRoomsPlugin;
class VeyonMasterInterface;

class RoomManagerDialog : public QDialog
{
	Q_OBJECT
public:
	RoomManagerDialog( VeyonMasterInterface& master, ItlabRoomsPlugin* plugin );
	~RoomManagerDialog() override;

	// câu trả lời từ dịch vụ trên máy HS/GV (gọi từ ItlabRoomsPlugin)
	void handleReply( ComputerControlInterface::Pointer computerControlInterface, const FeatureMessage& message );

protected:
	void reject() override;

private:
	// ───── danh sách phòng/máy đang sửa (cùng định dạng JSON với BuiltinDirectory) ─────
	void loadDirectory();
	QString findLocationUid( const QString& roomName ) const;
	QString roomNameOf( const QString& locationUid ) const;
	QString ensureLocation( const QString& roomName );
	int findComputerIndex( const QString& name, const QString& hostAddress ) const;
	void addOrMoveComputer( const QString& name, const QString& hostAddress, const QString& macAddress,
							const QString& locationUid );
	QStringList roomNames() const;
	void setModified();

	// ───── giao diện ─────
	QWidget* createRoomsTab();
	QWidget* createPatternTab();
	QWidget* createDiscoveryTab();
	void refreshRoomsTree();
	void refreshRoomComboBoxes();
	void showStatus( const QString& text );

	// thẻ "Phòng và máy"
	void addRoom();
	void renameRoom();
	void removeSelected();
	void addComputersByName();
	void moveSelectedComputers();
	void writeRoomToSelectedComputers();

	// thẻ "Tạo theo mẫu tên"
	QStringList patternNames() const;
	void updatePatternPreview();
	void addPatternComputers();

	// thẻ "Tự dò máy"
	static QStringList parseAddressRanges( const QString& ranges );
	static QString defaultScanRange();
	void startDiscovery();
	void finishPortScan();
	void queryComputer( const QString& ipAddress );
	void finishDiscovery();
	void addDiscoveredComputers( bool byDeclaredRoom );

	// ghi tên phòng vào cấu hình máy HS từ xa
	void sendSetRoom( const QString& hostAddress, const QString& displayName, const QString& room );

	void save();

	struct DiscoveredComputer
	{
		QString ipAddress;
		QString hostName;
		QString room;
		QString user;
		QString macAddress;
		bool answered{false};
		bool nameResolvesToAddress{false};
		QTreeWidgetItem* item{nullptr};
		ComputerControlInterface::Pointer controlInterface;
	};

	VeyonMasterInterface& m_master;
	ItlabRoomsPlugin* m_plugin;

	QJsonArray m_objects;
	bool m_modified{false};

	// thẻ "Phòng và máy"
	QTreeWidget* m_roomsTree{nullptr};

	// thẻ "Tạo theo mẫu tên"
	QComboBox* m_patternRoom{nullptr};
	QLineEdit* m_patternPrefix{nullptr};
	QSpinBox* m_patternFrom{nullptr};
	QSpinBox* m_patternTo{nullptr};
	QSpinBox* m_patternDigits{nullptr};
	QLineEdit* m_patternSuffix{nullptr};
	QListWidget* m_patternPreview{nullptr};

	// thẻ "Tự dò máy"
	QLineEdit* m_scanRange{nullptr};
	QPushButton* m_scanButton{nullptr};
	QProgressBar* m_scanProgress{nullptr};
	QTreeWidget* m_discoveredTree{nullptr};
	QComboBox* m_discoveryRoom{nullptr};
	QCheckBox* m_writeRoomToComputers{nullptr};
	QList<QTcpSocket*> m_scanSockets;
	QStringList m_openAddresses;
	QMap<QString, DiscoveredComputer> m_discovered; // theo địa chỉ IP

	// kết nối tạm (đặt phòng cho máy chưa có trong danh sách)
	struct PendingSetRoom
	{
		ComputerControlInterface::Pointer controlInterface;
		QString displayName;
		QString room;
		bool sent{false};
	};
	QList<PendingSetRoom> m_pendingSetRoom;

	QLabel* m_statusLabel{nullptr};
	bool m_saving{false};

};
