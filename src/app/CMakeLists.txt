add_executable(iris
    main.cpp
    iris/application.cpp
    iris/application.hpp
)

target_include_directories(iris PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iris PRIVATE iris_frontend)
iris_set_warnings(iris)

set_target_properties(iris PROPERTIES
    OUTPUT_NAME iris
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
