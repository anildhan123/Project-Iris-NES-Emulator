add_library(iris_platform STATIC
    iris/paths.cpp
    iris/paths.hpp
)

target_include_directories(iris_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iris_platform PUBLIC iris_common)
iris_set_warnings(iris_platform)
