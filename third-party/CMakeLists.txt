# Optional third-party dependencies, vendored as git submodules.
#
# Initialize them with:
#     git submodule update --init --recursive
#
# Each is built only when its option is enabled, so the default configure and
# the asset-free CI stay dependency-free and fast.

if(RA2YR_ENABLE_MINIAUDIO)
    add_library(miniaudio INTERFACE)
    target_include_directories(miniaudio INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/miniaudio")
    add_library(third-party::miniaudio ALIAS miniaudio)
endif()

if(RA2YR_ENABLE_SDL3)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(SDL)
endif()

if(RA2YR_ENABLE_BGFX)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bgfx.cmake/CMakeLists.txt")
        message(FATAL_ERROR
            "bgfx.cmake is not initialized. Run:\n"
            "  git submodule update --init --recursive")
    endif()
    set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BGFX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
    set(BGFX_CONFIG_VIDEO OFF CACHE BOOL "" FORCE)
    set(BGFX_CONFIG_RENDERER_WEBGPU OFF CACHE BOOL "" FORCE)
    add_subdirectory(bgfx.cmake)
endif()
