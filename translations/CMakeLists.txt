include(CreateTranslations)
include(ImportQtTranslations)

# ITLabControl: only Vietnamese is maintained (English is the source language). Processing just this
# file keeps the other upstream .ts files untouched by builds, which avoids spurious git changes.
set(veyon_translations ${CMAKE_CURRENT_SOURCE_DIR}/veyon_vi.ts)
file(GLOB_RECURSE veyon_sources ${CMAKE_SOURCE_DIR}/*.cpp ${CMAKE_SOURCE_DIR}/*.h ${CMAKE_SOURCE_DIR}/*.ui ${CMAKE_SOURCE_DIR}/*.qml)
string(REGEX REPLACE "${CMAKE_SOURCE_DIR}/3rdparty[^;]+;?" "" veyon_sources "${veyon_sources}")
string(REGEX REPLACE "${CMAKE_SOURCE_DIR}/addons[^;]+;?" "" veyon_sources "${veyon_sources}")

create_translations(veyon "${veyon_translations}" "${veyon_sources}")
import_qt_translations()

# ITLabControl: Qt does not ship a Vietnamese translation, so provide one for the most visible
# Qt strings (standard dialog buttons etc.). It is loaded as "qtbase" by TranslationLoader.
if(WITH_TRANSLATIONS)
	set(qtbase_vi_qm "${CMAKE_CURRENT_BINARY_DIR}/qtbase_vi.qm")
	add_custom_command(OUTPUT ${qtbase_vi_qm}
		COMMAND Qt${QT_MAJOR_VERSION}::lrelease ${CMAKE_CURRENT_SOURCE_DIR}/qtbase_vi.ts -qm ${qtbase_vi_qm}
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/qtbase_vi.ts)
	add_custom_target(qtbase-vi-translation ALL DEPENDS ${qtbase_vi_qm})
	install(FILES ${qtbase_vi_qm} DESTINATION ${VEYON_INSTALL_DATA_DIR}/translations)
endif()
