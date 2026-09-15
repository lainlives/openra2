# The engine is split into a platform-free core library plus thin executables.
# Nothing here may link a proprietary SDK or require retail game data to build.

file(GLOB RA2YR_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/platform/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/vfs/*.cpp"
)

add_library(ra2yr_core STATIC ${RA2YR_CORE_SOURCES})
target_include_directories(ra2yr_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(ra2yr "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(ra2yr PRIVATE ra2yr_core)
