project(kwebkitpart)

# search packages used by KDE
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)

add_subdirectory(cmake)
add_subdirectory(icons)
add_subdirectory(src)
add_subdirectory(kdelauncher)
