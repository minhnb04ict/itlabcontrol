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
		RestrictionsBackend.h
		WebsiteListDialog.cpp
		WebsiteListDialog.h
		itlab-restrictions.qrc
		)
	target_link_libraries(itlabrestrictions PRIVATE -luserenv)
endif()
