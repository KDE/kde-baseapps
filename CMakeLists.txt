project(webkitkde)

# search packages used by KDE
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kdewebkit) # required; otherwise kdewebkit_export.h cannot be found when building from toplevel (just temporary until kdewebkit is in kdelibs) -uwolfer

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/kdewebkit/cmake/modules
    ${CMAKE_SOURCE_DIR}/part/cmake/modules)

add_subdirectory(icons)
if(${KDE_VERSION} VERSION_LESS 4.3.74)
  add_subdirectory(kdewebkit)
endif(${KDE_VERSION} VERSION_LESS 4.3.74)
add_subdirectory(part)
add_subdirectory(kdelauncher)
