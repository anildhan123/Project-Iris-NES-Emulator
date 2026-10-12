add_library(iris_core STATIC
    iris.cpp
    iris.h
)

target_include_directories(iris_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iris_core PUBLIC iris_nes)
iris_set_warnings(iris_core)
