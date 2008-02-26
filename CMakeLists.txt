project(webkitkde)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules )

# search packages used by KDE
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)

include_directories(${KDE4_INCLUDES})
add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})

set(webkitkde_LIB_SRCS
    webkitpart.cpp
#     kwebnetworkinterface.cpp
)

kde4_add_plugin(webkitkde ${webkitkde_LIB_SRCS})

target_link_libraries(webkitkde ${KDE4_KPARTS_LIBS} ${QT_QTWEBKIT_LIBRARY})

install(TARGETS webkitkde DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES webkitpart.desktop DESTINATION ${SERVICES_INSTALL_DIR})
