project(plasma-folderview)

set(folderview_SRCS
    folderview.cpp
    abstractitemview.cpp
    listview.cpp
    dialog.cpp
    label.cpp
    iconview.cpp
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

target_link_libraries(plasma_applet_folderview ${KDE4_PLASMA_LIBS} konq ${KDE4_KIO_LIBS} ${KDE4_KFILE_LIBS} ${X11_LIBRARIES})

install(TARGETS plasma_applet_folderview DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES plasma-applet-folderview.desktop DESTINATION ${SERVICES_INSTALL_DIR})

