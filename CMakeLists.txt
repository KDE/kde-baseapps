project(kwebkitpart)

# search packages used by KDE
find_package(KDE4 4.8.3 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)


add_subdirectory(icons)
add_subdirectory(src)
add_subdirectory(kdelauncher)
