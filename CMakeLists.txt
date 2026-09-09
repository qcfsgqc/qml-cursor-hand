cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(CURSORHAND_IS_TOPLEVEL ON)
    project(CursorHand VERSION 1.0.0 LANGUAGES CXX)
else()
    set(CURSORHAND_IS_TOPLEVEL OFF)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(NOT TARGET Qt6::Quick)
    find_package(Qt6 6.10 REQUIRED COMPONENTS Quick)
endif()

if(CURSORHAND_IS_TOPLEVEL)
    qt_standard_project_setup(REQUIRES 6.10)
endif()

option(CURSORHAND_BUILD_EXAMPLE "Build the example application" ${CURSORHAND_IS_TOPLEVEL})

add_subdirectory(src)

if(NOT TARGET Cursor::Hand)
    add_library(Cursor::Hand ALIAS cursorhand)
endif()

# Link Cursor.Hand into an app/QML module. On WASM / static Qt this also
# pulls in the QML plugin via qt_import_qml_plugins().
function(cursorhand_link target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "cursorhand_link: '${target}' is not a CMake target")
    endif()
    target_link_libraries(${target} PRIVATE cursorhand)
    if(EMSCRIPTEN OR (DEFINED QT6_IS_SHARED_LIBS_BUILD AND NOT QT6_IS_SHARED_LIBS_BUILD))
        if(COMMAND qt_import_qml_plugins)
            qt_import_qml_plugins(${target})
        endif()
    endif()
endfunction()

if(CURSORHAND_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()
