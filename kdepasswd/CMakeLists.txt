cmake_minimum_required(VERSION 2.8.12)

project(Kdepasswd)

# ECM setup
find_package(ECM 1.7.0 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(ECMSetupVersion)
include(FeatureSummary)
include(ECMAddAppIcon)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)

set(KDEPASSWD_VERSION "4.70.0")

ecm_setup_version(${KDEPASSWD_VERSION} VARIABLE_PREFIX KDEPASSWD
                        VERSION_HEADER "${Kdepasswd_BINARY_DIR}/kdepasswd_version.h"
)

# Build dependencies
find_package(KF5 REQUIRED COMPONENTS DocTools KIO Su DBusAddons I18n)

add_definitions(-DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_URL_CAST_FROM_STRING)

set(kdepasswd_SRCS
    kdepasswd.cpp
    passwd.cpp
    passwddlg.cpp
    kdepassword_debug.cpp
)

add_executable(kdepasswd ${kdepasswd_SRCS})

target_link_libraries(kdepasswd
    KF5::KIOWidgets
    KF5::Su
    KF5::DBusAddons
    KF5::I18n
)

install(TARGETS kdepasswd ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS org.kde.kdepasswd.desktop DESTINATION ${KDE_INSTALL_APPDIR})

if("${CMAKE_SOURCE_DIR}" STREQUAL "${Kdepasswd_SOURCE_DIR}")
  feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()

