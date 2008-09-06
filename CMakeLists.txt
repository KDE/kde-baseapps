project(webkitkde)

set(CMAKE_MODULE_PATH
    ${CMAKE_SOURCE_DIR}/kdewebkit/cmake/modules
    ${CMAKE_SOURCE_DIR}/kdenetwork/cmake/modules
    ${CMAKE_SOURCE_DIR}/part/cmake/modules)

add_subdirectory(kdenetwork)
add_subdirectory(icons)
add_subdirectory(kdewebkit)
add_subdirectory(part)
