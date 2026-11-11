# itlab-cleardesktop - dọn màn hình máy học sinh (thu nhỏ hoặc đóng ứng dụng)
#
# Chỉ hỗ trợ Windows (dùng Win32 API để liệt kê và xử lý cửa sổ).

include(BuildVeyonPlugin)

if(VEYON_BUILD_WINDOWS)
	build_veyon_plugin(itlabcleardesktop
		NAME ItlabClearDesktop
		SOURCES
		ItlabClearDesktopFeaturePlugin.cpp
		ItlabClearDesktopFeaturePlugin.h
		itlab-cleardesktop.qrc
		)
endif()
