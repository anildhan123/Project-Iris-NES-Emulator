add_executable(iris_tests
    cartridge_tests.cpp
    conformance_tests.cpp
    crash_tests.cpp
    regression_tests.cpp
    config_tests.cpp
    cpu_tests.cpp
    log_tests.cpp
    library_tests.cpp
    nes_system_tests.cpp
    core_tests.cpp
    paths_tests.cpp
    snes_tests.cpp
    video_tests.cpp
)

target_include_directories(iris_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/ports/ios)
target_link_libraries(iris_tests PRIVATE iris_common iris_platform iris_nes iris_snes iris_core Catch2::Catch2WithMain)
iris_set_warnings(iris_tests)

set_target_properties(iris_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME iris_unit COMMAND iris_tests)
if(TARGET iris)
    add_test(
        NAME iris_profile
        COMMAND iris --profile $<TARGET_FILE_DIR:iris>/iris_demo.nes
    )
    set_tests_properties(iris_profile PROPERTIES
        ENVIRONMENT "IRIS_DATA_DIR=${CMAKE_BINARY_DIR}/profile-data"
    )
endif()
