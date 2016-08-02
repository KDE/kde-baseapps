cmake_minimum_required(VERSION 2.8.12)

project(keditbookmarks)

# ECM setup
find_package(ECM 1.7.0 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(ECMOptionalAddSubdirectory)
include(ECMInstallIcons)
include(ECMSetupVersion)
include(ECMAddTests)
include(ECMMarkNonGuiExecutable)

include(CMakePackageConfigHelpers)
include(FeatureSummary)
include(ECMAddAppIcon)
include(CheckFunctionExists)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)

set(KEDITBOOKMARKS_VERSION "5.97.0")

find_package(Qt5 REQUIRED COMPONENTS Core)
find_package(KF5 REQUIRED COMPONENTS CoreAddons I18n KIO KDELibs4Support)

add_definitions(-DQT_USE_FAST_CONCATENATION)
remove_definitions(-DQT_NO_URL_CAST_FROM_STRING)

############ sources ###################

add_subdirectory(kbookmarkmodel)

########### next target ###############

set(kbookmarkmerger_SRCS kbookmarkmerger.cpp )

add_executable(kbookmarkmerger ${kbookmarkmerger_SRCS})

target_link_libraries(kbookmarkmerger  KF5::KIOCore KF5::KDELibs4Support)

install(TARGETS kbookmarkmerger ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})


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

kconfig_add_kcfg_files(keditbookmarks_KDEINIT_SRCS settings.kcfgc )

kde4_add_kdeinit_executable( keditbookmarks ${keditbookmarks_KDEINIT_SRCS})

target_link_libraries(kdeinit_keditbookmarks kbookmarkmodel_private
   KF5::Parts
   KF5::Bookmarks
   KF5::WindowSystem
   KF5::IconThemes
   KF5::KIOGui)

install(TARGETS kdeinit_keditbookmarks  ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(TARGETS keditbookmarks          ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

########### install files ###############

install( FILES keditbookmarks.kcfg  DESTINATION  ${KDE_INSTALL_KCFGDIR} )
install( FILES keditbookmarksui.rc  keditbookmarks-genui.rc  DESTINATION  ${KDE_INSTALL_KXMLGUI5DIR}/keditbookmarks )
install( PROGRAMS org.kde.keditbookmarks.desktop DESTINATION ${KDE_INSTALL_APPDIR} )


# TODO Remove the if/endif lines if keditbookmarks is split.
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${keditbookmarks_SOURCE_DIR}")
    feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()
