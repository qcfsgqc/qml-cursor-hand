cmake_minimum_required(VERSION 3.16)
project(CursorHand VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

find_package(Qt6 6.10 REQUIRED COMPONENTS Quick)
qt_standard_project_setup(REQUIRES 6.10)

option(CURSORHAND_BUILD_EXAMPLE "Build the example application" ON)

add_subdirectory(src)

if(CURSORHAND_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()
