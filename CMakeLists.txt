
project(webkitkde)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules )

# search packages used by KDE
find_package(KDE4 REQUIRED)
include (KDE4Defaults)
include (MacroLibrary)
include(MacroOptionalAddSubdirectory)

include(FindPkgConfig)
pkg_check_modules(QtWebKit REQUIRED QtWebKit)

include_directories(${QtWebKit_INCLUDE_DIRS} ${KDE4_INCLUDES})
link_directories(${QtWebKit_LIBRARY_DIRS})
add_definitions(${KDE4_DEFINITIONS})

set(webkitkde_LIB_SRCS
    webkitpart.cpp
    kwebnetworkinterface.cpp
)

kde4_automoc(${webkitkde_LIB_SRCS})

kde4_add_plugin(webkitkde WITH_PREFIX ${webkitkde_LIB_SRCS})

target_link_libraries(webkitkde ${KDE4_KPARTS_LIBS} ${QtWebKit_LIBRARIES})

install(TARGETS webkitkde DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES webkitpart.desktop DESTINATION ${SERVICES_INSTALL_DIR})
