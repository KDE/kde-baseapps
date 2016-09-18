cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)

project(KDE_BASEAPPS)

set(QT_MIN_VERSION "5.4.0")
set(KF5_MIN_VERSION "5.24.0")

include(FeatureSummary)

add_subdirectory(lib)
add_subdirectory(kdialog)
add_subdirectory(doc)
if(NOT WIN32)
    add_subdirectory(kdepasswd)
endif()
add_subdirectory(kfind)

add_subdirectory(keditbookmarks)
add_subdirectory(konqueror)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
