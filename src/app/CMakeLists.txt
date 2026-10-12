add_executable(iris
    main.cpp
    iris/application.cpp
    iris/application.hpp
)

target_include_directories(iris PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iris PRIVATE iris_frontend iris_nes)
iris_set_warnings(iris)

add_dependencies(iris iris_make_demo)
add_custom_command(TARGET iris POST_BUILD
    COMMAND $<TARGET_FILE:iris_make_demo> "$<TARGET_FILE_DIR:iris>/iris_demo.nes"
    VERBATIM
)

set_target_properties(iris PROPERTIES
    OUTPUT_NAME iris
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
