cmake_minimum_required(VERSION 3.24)

project(bmsonto62bms
    VERSION 0.1.1
    DESCRIPTION "Converts bmson charts into extended (base-62) BMS with sliced sounds"
    HOMEPAGE_URL "https://github.com/Much-Ryouma/bmsonto62bms"
    LANGUAGES C CXX)

option(B2B_BUILD_GUI   "Build the Qt GUI application" ON)
option(B2B_BUILD_CLI   "Build the command line tool" ON)
option(B2B_BUILD_TESTS "Build unit tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Dependencies)
include(Vorbis)
include(WindowsResources)

add_subdirectory(src/core)
add_subdirectory(src/infra)
add_subdirectory(src/app)

if(B2B_BUILD_CLI)
    add_subdirectory(src/cli)
endif()

if(B2B_BUILD_GUI)
    add_subdirectory(src/gui)
endif()

if(B2B_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(Packaging)
