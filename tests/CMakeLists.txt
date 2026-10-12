add_executable(iris_tests
    cartridge_tests.cpp
    conformance_tests.cpp
    config_tests.cpp
    cpu_tests.cpp
    log_tests.cpp
    nes_system_tests.cpp
    paths_tests.cpp
)

target_include_directories(iris_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iris_tests PRIVATE iris_common iris_platform iris_nes Catch2::Catch2WithMain)
iris_set_warnings(iris_tests)

set_target_properties(iris_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME iris_unit COMMAND iris_tests)
