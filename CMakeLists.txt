project(KDE_BASEAPPS)

cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

set(QT_MIN_VERSION "5.4.0")
set(KF5_MIN_VERSION "5.19.0")

find_package(ECM 1.7.0 REQUIRED CONFIG)

include(FeatureSummary)

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMInstallIcons)
include(ECMSetupVersion)

set(KDE_BASEAPPS_VERSION "4.97.0")

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core DBus Gui Widgets)
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED Archive Bookmarks CoreAddons Config ConfigWidgets DBusAddons KIO KDELibs4Support Parts Activities KDELibs4Support)
if(NOT WIN32)
    find_package(KF5 ${KF5_MIN_VERSION} REQUIRED Su)
endif()

find_package(ZLIB)
set_package_properties(ZLIB PROPERTIES DESCRIPTION "Support for gzip compressed files and data streams"
                       URL "http://www.zlib.net"
                       TYPE REQUIRED
                       PURPOSE "Required for building Konqueror"
                      )

configure_file (config-apps.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-apps.h )


add_definitions (-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)

add_subdirectory(lib)
add_subdirectory(kdialog)
add_subdirectory(doc)
if(NOT WIN32)
    add_subdirectory(kdepasswd)
endif()
add_subdirectory(kfind)

add_subdirectory( keditbookmarks )
add_subdirectory(konqueror)
add_subdirectory( konq-plugins )
# if ( Q_WS_X11 )
#   add_subdirectory( nsplugins )
# endif ()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
