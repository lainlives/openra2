cmake_minimum_required(VERSION 3.24)

project(ra2yr
    VERSION 0.1.0
    DESCRIPTION "Standalone reimplementation of the Red Alert 2 / Yuri's Revenge engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(RA2YR_BUILD_TESTS "Build the test suite" ON)
option(RA2YR_ENABLE_SDL3 "Build the SDL3 window/input backend" OFF)
option(RA2YR_ENABLE_BGFX "Build the bgfx rendering backend" OFF)
option(RA2YR_ENABLE_MINIAUDIO "Use the vendored miniaudio for audio" OFF)

add_subdirectory(third-party)
add_subdirectory(src)

if(RA2YR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
