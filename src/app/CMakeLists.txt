add_executable(iris
    main.cpp
    iris/application.cpp
    iris/application.hpp
    iris/crash.cpp
    iris/crash.hpp
)

target_include_directories(iris PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iris PRIVATE iris_frontend iris_nes iris_snes)
iris_set_warnings(iris)

set(IRIS_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(IRIS_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(IRIS_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(IRIS_VERSION_STRING ${PROJECT_VERSION})
configure_file(iris.rc.in ${CMAKE_CURRENT_BINARY_DIR}/iris.rc @ONLY)
if(WIN32)
    target_sources(iris PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/iris.rc)
endif()

add_dependencies(iris iris_make_demo iris_make_snes_demo)
add_custom_command(TARGET iris POST_BUILD
    COMMAND $<TARGET_FILE:iris_make_demo> "$<TARGET_FILE_DIR:iris>/iris_demo.nes"
    COMMAND $<TARGET_FILE:iris_make_snes_demo> "$<TARGET_FILE_DIR:iris>/iris_demo.sfc"
    VERBATIM
)

set_target_properties(iris PROPERTIES
    OUTPUT_NAME iris
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
