add_executable(test_core test_core.cpp)
target_link_libraries(test_core PRIVATE ra2yr_core)
add_test(NAME core COMMAND test_core)

add_executable(test_mix test_mix.cpp)
target_link_libraries(test_mix PRIVATE ra2yr_core)
add_test(NAME mix COMMAND test_mix)