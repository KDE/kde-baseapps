cmake_minimum_required(VERSION 2.8.12)

# ECM setup
find_package(ECM 1.2.0 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(ECMOptionalAddSubdirectory)
include(ECMInstallIcons)
include(ECMSetupVersion)
include(ECMAddTests)
include(ECMMarkNonGuiExecutable)

include(CMakePackageConfigHelpers)
include(FeatureSummary)
include(CheckFunctionExists)

include(KDEInstallDirs)
include(KDECMakeSettings)


add_subdirectory(kbookmarkmodel)

########### next target ###############

set(kbookmarkmerger_SRCS kbookmarkmerger.cpp )

add_executable(kbookmarkmerger ${kbookmarkmerger_SRCS})

target_link_libraries(kbookmarkmerger  KF5::KIOCore KF5::KDELibs4Support)

install(TARGETS kbookmarkmerger ${INSTALL_TARGETS_DEFAULT_ARGS})


########### next target ###############

set(keditbookmarks_KDEINIT_SRCS 
   main.cpp
   toplevel.cpp
   globalbookmarkmanager.cpp
   actionsimpl.cpp
   importers.cpp
   bookmarkiterator.cpp
   testlink.cpp
   favicons.cpp
   faviconupdater.cpp
   exporters.cpp
   bookmarkinfowidget.cpp
   kebsearchline.cpp
   bookmarklistview.cpp
)

set(toplevel_xml ${CMAKE_CURRENT_BINARY_DIR}/org.kde.keditbookmarks.xml)
qt5_generate_dbus_interface( ${CMAKE_CURRENT_SOURCE_DIR}/toplevel.h org.kde.keditbookmarks.xml )

qt5_add_dbus_interface( keditbookmarks_KDEINIT_SRCS ${toplevel_xml} toplevel_interface )
qt5_add_dbus_interface( keditbookmarks_KDEINIT_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../lib/konq/src/favicons/org.kde.FavIcon.xml favicon_interface)


kconfig_add_kcfg_files(keditbookmarks_KDEINIT_SRCS settings.kcfgc )

kde4_add_kdeinit_executable( keditbookmarks ${keditbookmarks_KDEINIT_SRCS})

target_link_libraries(kdeinit_keditbookmarks kbookmarkmodel_private KF5::Konq KF5::Parts)

install(TARGETS kdeinit_keditbookmarks  ${INSTALL_TARGETS_DEFAULT_ARGS})
install(TARGETS keditbookmarks          ${INSTALL_TARGETS_DEFAULT_ARGS})

########### install files ###############

install( FILES keditbookmarks.kcfg  DESTINATION  ${KCFG_INSTALL_DIR} )
install( FILES keditbookmarksui.rc  keditbookmarks-genui.rc  DESTINATION  ${DATA_INSTALL_DIR}/keditbookmarks )
install( PROGRAMS org.kde.keditbookmarks.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} )
