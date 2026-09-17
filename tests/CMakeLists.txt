add_executable(test_core test_core.cpp)
target_link_libraries(test_core PRIVATE ra2yr_core)
add_test(NAME core COMMAND test_core)

add_executable(test_mix test_mix.cpp)
target_link_libraries(test_mix PRIVATE ra2yr_core)
add_test(NAME mix COMMAND test_mix)

add_executable(test_formats test_formats.cpp)
target_link_libraries(test_formats PRIVATE ra2yr_core)
add_test(NAME formats COMMAND test_formats)
add_executable(test_data test_data.cpp)
target_link_libraries(test_data PRIVATE ra2yr_core)
target_compile_definitions(test_data PRIVATE
    RA2YR_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME data COMMAND test_data)
