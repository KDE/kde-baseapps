project(plasma-folderview)


if ( NOT Q_WS_MAC )
    if (EXISTS "${kdebase_SOURCE_DIR}" AND EXISTS "${kdebase_SOURCE_DIR}/workspace")
        set (KDE4WORKSPACE_FOUND TRUE)
        set (KDE4WORKSPACE_KWORKSPACE_LIBRARY_INCLUDE_DIR ${kdebase_SOURCE_DIR}/workspace/libs)
        set (KDE4WORKSPACE_KWORKSPACE_LIBS kworkspace)
    else (EXISTS "${kdebase_SOURCE_DIR}" AND EXISTS "${kdebase_SOURCE_DIR}/workspace")
        macro_optional_find_package(KDE4Workspace)
    endif (EXISTS "${kdebase_SOURCE_DIR}" AND EXISTS "${kdebase_SOURCE_DIR}/workspace")
endif ( NOT Q_WS_MAC )

include (ConfigureChecks.cmake)
configure_file(config-folderview.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-folderview.h)

include_directories (${KDE4WORKSPACE_KWORKSPACE_LIBRARY_INCLUDE_DIR})

set(folderview_SRCS
    folderview.cpp
    abstractitemview.cpp
    listview.cpp
    dialog.cpp
    label.cpp
    iconview.cpp
    popupview.cpp
    iconwidget.cpp
    dirlister.cpp
    proxymodel.cpp
    folderviewadapter.cpp
    previewpluginsmodel.cpp
    tooltipwidget.cpp
    style.cpp)

kde4_add_ui_files(folderview_SRCS
                  folderviewFilterConfig.ui
                  folderviewDisplayConfig.ui
                  folderviewLocationConfig.ui
                  folderviewPreviewConfig.ui)

kde4_add_plugin(plasma_applet_folderview ${folderview_SRCS})

target_link_libraries(plasma_applet_folderview ${KDE4_PLASMA_LIBS} konq ${KDE4WORKSPACE_KWORKSPACE_LIBS} 
                      ${KDE4_KIO_LIBS} ${KDE4_KFILE_LIBS} ${KDE4_SOLID_LIBS} ${X11_LIBRARIES})

install(TARGETS plasma_applet_folderview DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES plasma-applet-folderview.desktop DESTINATION ${SERVICES_INSTALL_DIR})

