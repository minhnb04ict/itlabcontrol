# itlab-audio - tắt/bật tiếng và chỉnh âm lượng máy học sinh (Windows Core Audio)

include(BuildVeyonPlugin)

if(VEYON_BUILD_WINDOWS)
	build_veyon_plugin(itlabaudio
		NAME ItlabAudio
		SOURCES
		ItlabAudioPlugin.cpp
		ItlabAudioPlugin.h
		itlab-audio.qrc
		)
	target_link_libraries(itlabaudio PRIVATE -lole32 -luuid)
endif()
