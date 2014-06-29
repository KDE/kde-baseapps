cmake_minimum_required(VERSION 2.8.12)

project(Kdepasswd)

# ECM setup
find_package(ECM 0.0.14 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(ECMSetupVersion)
include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings)

set(KDEPASSWD_VERSION "4.70.0")

ecm_setup_version(${KDEPASSWD_VERSION} VARIABLE_PREFIX KDEPASSWD
                        VERSION_HEADER "${Kdepasswd_BINARY_DIR}/kdepasswd_version.h"
)

# Build dependencies
find_package(KF5 REQUIRED COMPONENTS KDELibs4Support DocTools KIO Su)

if ("${CMAKE_SOURCE_DIR}" STREQUAL "${Kdepasswd_SOURCE_DIR}")
    find_package(KF5Konq CONFIG REQUIRED)
else()
    include_directories(${LibKonq_SOURCE_DIR}/src)
endif()

remove_definitions(-DQT_NO_CAST_FROM_BYTEARRAY -DQT_NO_URL_CAST_FROM_STRING)

set(kdepasswd_SRCS
    kdepasswd.cpp
    passwd.cpp
    passwddlg.cpp)

add_executable(kdepasswd ${kdepasswd_SRCS})

target_link_libraries(kdepasswd
    KF5::KIOWidgets
    KF5::Su
    KF5::KDELibs4Support
)

install(TARGETS kdepasswd ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})
install(PROGRAMS kdepasswd.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})

add_subdirectory(kcm)
add_subdirectory(docs)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${Kdepasswd_SOURCE_DIR}")
  feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()

