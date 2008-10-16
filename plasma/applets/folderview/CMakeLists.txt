project(plasma-folderview)

# Find the required Libaries
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
find_package(Plasma REQUIRED)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
   ${KDE4_INCLUDES}
   )

set(folderview_SRCS
    folderview.cpp
    dirlister.cpp
    proxymodel.cpp
    folderviewadapter.cpp
    previewpluginsmodel.cpp)

kde4_add_ui_files(folderview_SRCS
                  folderviewFilterConfig.ui
                  folderviewDisplayConfig.ui
                  folderviewLocationConfig.ui
                  folderviewPreviewConfig.ui)

kde4_add_plugin(plasma_applet_folderview ${folderview_SRCS})
target_link_libraries(plasma_applet_folderview ${PLASMA_LIBS} konq ${KDE4_KIO_LIBS} ${KDE4_KFILE_LIBS} ${X11_LIBRARIES})

install(TARGETS plasma_applet_folderview DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES plasma-applet-folderview.desktop DESTINATION ${SERVICES_INSTALL_DIR})

