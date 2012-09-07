project(plasma-applet-dirlister)

include (KDE4Defaults)

include_directories(
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  )

set(dirlister_SRCS
    dirlisterapplet.cpp
    ../proxymodel.cpp
  )

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})

kde4_add_plugin(plasma_applet_dirlister ${dirlister_SRCS})
target_link_libraries(plasma_applet_dirlister ${KDE4_PLASMA_LIBS} ${KDE4_KIO_LIBS} ${KDE4_KFILE_LIBS} ${QT_QTDECLARATIVE_LIBRARY})

install(TARGETS plasma_applet_dirlister DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES plasma-applet-dirlister.desktop DESTINATION ${SERVICES_INSTALL_DIR})

install(DIRECTORY package/ DESTINATION ${DATA_INSTALL_DIR}/plasma/packages/org.kde.plasma.filemanagement)
install(FILES package/metadata.desktop DESTINATION ${SERVICES_INSTALL_DIR} RENAME plasma-package-org.kde.plasma.filemanagement.desktop)
