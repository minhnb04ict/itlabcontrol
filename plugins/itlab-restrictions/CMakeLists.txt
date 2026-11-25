# itlab-restrictions - hạn chế máy học sinh (GĐ2): chặn web, sau này chặn ứng dụng, USB, in, âm thanh
#
# Chỉ hỗ trợ Windows (chính sách trình duyệt trong registry, Windows Firewall).

include(BuildVeyonPlugin)

if(VEYON_BUILD_WINDOWS)
	build_veyon_plugin(itlabrestrictions
		NAME ItlabRestrictions
		SOURCES
		ItlabRestrictionsPlugin.cpp
		ItlabRestrictionsPlugin.h
		RestrictionsBackend.cpp
		RestrictionsBackendApps.cpp
		RestrictionsBackend.h
		WebsiteListDialog.cpp
		WebsiteListDialog.h
		AppListDialog.cpp
		AppListDialog.h
		RestrictionListsDialog.cpp
		RestrictionListsDialog.h
		WebHistory.cpp
		WebHistory.h
		WebHistoryDialog.cpp
		WebHistoryDialog.h
		itlab-restrictions.qrc
		)
	find_package(Qt${QT_MAJOR_VERSION} COMPONENTS Sql REQUIRED)
	target_link_libraries(itlabrestrictions PRIVATE -luserenv -liphlpapi -lws2_32 Qt${QT_MAJOR_VERSION}::Sql)
endif()
