# The engine is split into a platform-free core library plus thin executables.
# Nothing here may link a proprietary SDK or require retail game data to build.

file(GLOB RA2YR_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/formats/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/render/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vfs/*.cpp"
)

# Backend translation units are added only when their option is enabled, so the
# default build never needs their headers.
list(REMOVE_ITEM RA2YR_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/platform/sdl_backend.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/render/bgfx_renderer.cpp"
)

add_library(ra2yr_core STATIC ${RA2YR_CORE_SOURCES})
target_include_directories(ra2yr_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

if(RA2YR_ENABLE_SDL3)
    target_sources(ra2yr_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/platform/sdl_backend.cpp")
    target_compile_definitions(ra2yr_core PUBLIC RA2YR_PLATFORM_SDL)
    target_link_libraries(ra2yr_core PUBLIC SDL3::SDL3)
endif()

if(RA2YR_ENABLE_BGFX)
    set(RA2YR_BGFX_DIR "${CMAKE_SOURCE_DIR}/third-party/bgfx.cmake")
    set(RA2YR_SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/render/shaders")
    set(RA2YR_SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/generated/shaders")
    set(BGFX_SHADER_INCLUDE_PATH "${RA2YR_BGFX_DIR}/bgfx/src")
    include("${RA2YR_BGFX_DIR}/cmake/bgfxToolUtils.cmake")

    bgfx_compile_shaders(
        TYPE VERTEX
        SHADERS "${RA2YR_SHADER_SRC_DIR}/vs_tile.sc"
        VARYING_DEF "${RA2YR_SHADER_SRC_DIR}/varying.def.sc"
        OUTPUT_DIR "${RA2YR_SHADER_OUT_DIR}"
        OUT_FILES_VAR RA2YR_VS_TILE
        AS_HEADERS
    )
    bgfx_compile_shaders(
        TYPE FRAGMENT
        SHADERS "${RA2YR_SHADER_SRC_DIR}/fs_tile.sc"
        VARYING_DEF "${RA2YR_SHADER_SRC_DIR}/varying.def.sc"
        OUTPUT_DIR "${RA2YR_SHADER_OUT_DIR}"
        OUT_FILES_VAR RA2YR_FS_TILE
        AS_HEADERS
    )
    set_source_files_properties(${RA2YR_VS_TILE} ${RA2YR_FS_TILE} PROPERTIES GENERATED TRUE)

    target_sources(ra2yr_core PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/render/bgfx_renderer.cpp"
        ${RA2YR_VS_TILE}
        ${RA2YR_FS_TILE}
    )
    target_include_directories(ra2yr_core PRIVATE "${RA2YR_SHADER_OUT_DIR}")
    target_compile_definitions(ra2yr_core PUBLIC RA2YR_RENDER_BGFX)
    target_link_libraries(ra2yr_core PUBLIC bgfx)
endif()

add_executable(ra2yr "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(ra2yr PRIVATE ra2yr_core)