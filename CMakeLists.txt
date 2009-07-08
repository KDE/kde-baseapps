project(webkitkde)

# search packages used by KDE
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/kdewebkit/cmake/modules
    ${CMAKE_SOURCE_DIR}/part/cmake/modules)

if(${KDE_VERSION_MINOR} EQUAL "2" AND ${KDE_VERSION_RELEASE} GREATER "69")
else(${KDE_VERSION_MINOR} EQUAL "2" AND ${KDE_VERSION_RELEASE} GREATER "69")	
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/kdenetwork/cmake/modules)
    add_subdirectory(kdenetwork)
endif(${KDE_VERSION_MINOR} EQUAL "2" AND ${KDE_VERSION_RELEASE} GREATER "69")

add_subdirectory(icons)
add_subdirectory(kdewebkit)
add_subdirectory(part)
add_subdirectory(kdelauncher)
