# itlab-hello - plugin mẫu của ITLabControl (dùng để học, xem docs/LEARNING.md)
#
# Mặc định KHÔNG build và không có trong bộ cài, để nút "Xin chào" không hiện trên
# Bảng điều khiển giáo viên. Muốn build để học/thử nghiệm:
#   cmake -DWITH_ITLAB_SAMPLES=ON .
#
# build_veyon_plugin(<tên> SOURCES ...) là macro của Veyon
# (cmake/modules/BuildVeyonPlugin.cmake): tạo file itlabhello.dll,
# liên kết với veyon-core và cài vào thư mục plugins.
#
# Mọi plugin itlab-* đều khai báo theo đúng mẫu này.

option(WITH_ITLAB_SAMPLES "Build ITLabControl sample plugins (itlab-hello)" OFF)

if(WITH_ITLAB_SAMPLES)
	include(BuildVeyonPlugin)

	build_veyon_plugin(itlabhello
		NAME ItlabHello
		SOURCES
		ItlabHelloFeaturePlugin.cpp
		ItlabHelloFeaturePlugin.h
		itlab-hello.qrc
		)
endif()
