add_library(spice_core STATIC
    component.cpp
    circuit.cpp
    parser.cpp
)
target_include_directories(spice_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_compile_features(spice_core PUBLIC cxx_std_17)
spice_set_warnings(spice_core)

add_executable(spice_parser main.cpp)
target_link_libraries(spice_parser PRIVATE spice_core)
spice_set_warnings(spice_parser)
