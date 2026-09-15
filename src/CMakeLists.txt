# The engine is split into a platform-free core library plus thin executables.
# Nothing here may link a proprietary SDK or require retail game data to build.

file(GLOB RA2YR_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vfs/*.cpp"
)

# Backend translation units are added only when their option is enabled, so the
# default build never needs their headers.
list(REMOVE_ITEM RA2YR_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/platform/sdl_backend.cpp"
)

add_library(ra2yr_core STATIC ${RA2YR_CORE_SOURCES})
target_include_directories(ra2yr_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

if(RA2YR_ENABLE_SDL3)
    target_sources(ra2yr_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/platform/sdl_backend.cpp")
    target_compile_definitions(ra2yr_core PUBLIC RA2YR_PLATFORM_SDL)
    target_link_libraries(ra2yr_core PUBLIC SDL3::SDL3)
endif()

if(RA2YR_ENABLE_BGFX)
    target_sources(ra2yr_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/render/bgfx_renderer.cpp")
    target_compile_definitions(ra2yr_core PUBLIC RA2YR_RENDER_BGFX)
    target_link_libraries(ra2yr_core PUBLIC bgfx)
endif()

add_executable(ra2yr "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(ra2yr PRIVATE ra2yr_core)