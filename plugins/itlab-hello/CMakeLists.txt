# itlab-hello - plugin mẫu của ITLabControl
#
# build_veyon_plugin(<tên> SOURCES ...) là macro của Veyon
# (cmake/modules/BuildVeyonPlugin.cmake): tạo file itlabhello.dll,
# liên kết với veyon-core và cài vào thư mục plugins.
#
# Mọi plugin itlab-* đều khai báo theo đúng mẫu này.

include(BuildVeyonPlugin)

build_veyon_plugin(itlabhello
	NAME ItlabHello
	SOURCES
	ItlabHelloFeaturePlugin.cpp
	ItlabHelloFeaturePlugin.h
	itlab-hello.qrc
	)
