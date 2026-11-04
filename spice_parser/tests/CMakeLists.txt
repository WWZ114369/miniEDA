add_executable(spice_tests test_main.cpp)
target_link_libraries(spice_tests PRIVATE spice_core)
target_include_directories(spice_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
spice_set_warnings(spice_tests)

add_test(NAME SpiceUnitTests COMMAND spice_tests)
