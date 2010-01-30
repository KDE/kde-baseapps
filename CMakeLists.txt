project(kwebkitpart)

# search packages used by KDE
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)

if(${KDE_VERSION} VERSION_LESS 4.3.80)
  message("Building with kdewebkit lib from playground...")
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/kdewebkit) # required; otherwise kdewebkit_export.h cannot be found when building from toplevel (just temporary until kdewebkit is in kdelibs) -uwolfer
  add_subdirectory(kdewebkit)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/kdewebkit/cmake/modules)
endif(${KDE_VERSION} VERSION_LESS 4.3.80)

add_subdirectory(cmake)
add_subdirectory(icons)
add_subdirectory(src)
add_subdirectory(kdelauncher)
