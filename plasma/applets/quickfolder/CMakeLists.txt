project(plasma-quickfolder)

if(Q_WS_X11)
  find_package(X11 REQUIRED)
endif(Q_WS_X11)

set(quickfolder_SRCS
    quickfolder.cpp
#    abstractitemview.cpp
#    listview.cpp
    dialog.cpp
    dialogshadows.cpp
    label.cpp
#    iconview.cpp
#    popupview.cpp
    iconwidget.cpp
    dirlister.cpp
    proxymodel.cpp
#    folderviewadapter.cpp
    previewpluginsmodel.cpp
#    tooltipwidget.cpp
    style.cpp
    itemeditor.cpp
#    animator.cpp
#    actionoverlay.cpp
    asyncfiletester.cpp
    mimemodels.cpp
    remotewallpapersetter.cpp)

kde4_add_ui_files(quickfolder_SRCS
                  folderviewFilterConfig.ui
                  folderviewDisplayConfig.ui
                  folderviewLocationConfig.ui
                  folderviewPreviewConfig.ui)

kde4_add_plugin(plasma_applet_quickfolder ${quickfolder_SRCS})

target_link_libraries(plasma_applet_quickfolder ${KDE4_PLASMA_LIBS} konq
                      ${KDE4_KIO_LIBS} ${KDE4_KFILE_LIBS} ${KDE4_SOLID_LIBS} ${X11_LIBRARIES})

install(TARGETS plasma_applet_quickfolder DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES plasma-applet-quickfolder.desktop DESTINATION ${SERVICES_INSTALL_DIR})

