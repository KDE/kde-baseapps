project(KDEBASE_APPS)

cmake_minimum_required(VERSION 2.8.6 FATAL_ERROR)
set(QT_MIN_VERSION "5.2.0")

find_package(ECM 0.0.8 REQUIRED NO_MODULE)

include(FeatureSummary)


# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules )

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(ECMInstallIcons)

find_package(Qt5 CONFIG REQUIRED COMPONENTS Core DBus Test Widgets)
find_package(KF5 REQUIRED Archive Bookmarks CoreAddons Config ConfigWidgets DBusAddons KIO KDELibs4Support Parts Su Activities)

include(CTest)
include(CTestConfig.cmake)


find_package(ZLIB)
set_package_properties(ZLIB PROPERTIES DESCRIPTION "Support for gzip compressed files and data streams"
                       URL "http://www.zlib.net"
                       TYPE REQUIRED
                       PURPOSE "Required for building Konqueror"
                      )

include(ConfigureChecks.cmake)
configure_file (config-apps.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-apps.h )
include_directories (${CMAKE_CURRENT_BINARY_DIR})

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS} -DDISABLE_NEPOMUK_LEGACY)
add_definitions (-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/lib/konq ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

add_subdirectory( kioslave )
add_subdirectory( lib )
add_subdirectory( dolphin )
add_subdirectory( kdialog )
# add_subdirectory( keditbookmarks )
# add_subdirectory( konqueror )
add_subdirectory( kfind )
# add_subdirectory( plasma )
# add_subdirectory( konq-plugins )

# if ( Q_WS_MAC )
#   add_subdirectory( kdepasswd )
# endif ( Q_WS_MAC )

# if ( Q_WS_X11 )
#   add_subdirectory( nsplugins )
  add_subdirectory( kdepasswd )
# endif ( Q_WS_X11 )
# macro_optional_add_subdirectory( doc )

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

