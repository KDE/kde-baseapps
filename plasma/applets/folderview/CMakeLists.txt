project(plasma-folderview)

set(folderview_SRCS
    folderview.cpp
    proxymodel.cpp)

kde4_add_plugin(plasma_applet_folderview ${folderview_SRCS})
target_link_libraries(plasma_applet_folderview plasma ${KDE4_KIO_LIBS})
set_target_properties(plasma_applet_folderview PROPERTIES VERSION 1.0.0 SOVERSION 1)

install(TARGETS plasma_applet_folderview DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES plasma-folderview-default.desktop DESTINATION ${SERVICES_INSTALL_DIR})

