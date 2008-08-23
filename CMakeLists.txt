project(webkitkde)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# search packages used by KDE
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)

include_directories(${KDE4_INCLUDES})
add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})

add_subdirectory(cmake)
add_subdirectory(includes)
add_subdirectory(icons)
add_subdirectory(kdewebkit)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/kdewebkit/
)

set(webkitkde_LIB_SRCS
    webkitpart.cpp
    webview.cpp
    webpage.cpp
)

kde4_add_library(webkitkde SHARED ${webkitkde_LIB_SRCS})

target_link_libraries(webkitkde kdewebkit ${KDE4_KPARTS_LIBS})

set_target_properties(webkitkde PROPERTIES VERSION 1.0.0 SOVERSION 1)

install(TARGETS webkitkde ${INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES
    webkitkde_export.h
    webkitpart.h
    webview.h
    DESTINATION ${INCLUDE_INSTALL_DIR}/webkitkde/ COMPONENT Devel)


kde4_add_plugin(webkitkdepart webkitpartfactory.cpp)

target_link_libraries(webkitkdepart webkitkde)

install(TARGETS webkitkdepart DESTINATION ${PLUGIN_INSTALL_DIR})


install(FILES webkitpart.desktop DESTINATION ${SERVICES_INSTALL_DIR})

install(FILES webkitpart.rc DESTINATION  ${DATA_INSTALL_DIR}/webkitpart)
