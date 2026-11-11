# itlab-rooms - quản lý nhiều phòng máy: thêm máy theo tên/mẫu tên, tự dò máy,
# gán phòng cho máy học sinh (cả từ xa và trong Configurator)

include(BuildVeyonPlugin)

build_veyon_plugin(itlabrooms
	NAME ItlabRooms
	SOURCES
	ItlabRoomsPlugin.cpp
	ItlabRoomsPlugin.h
	RoomManagerDialog.cpp
	RoomManagerDialog.h
	RoomConfigurationPage.cpp
	RoomConfigurationPage.h
	itlab-rooms.qrc
	)
