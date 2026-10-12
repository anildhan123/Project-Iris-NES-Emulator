add_library(iris_common STATIC
    iris/config.cpp
    iris/config.hpp
    iris/error.hpp
    iris/log.cpp
    iris/log.hpp
    iris/version.hpp
)

target_include_directories(iris_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iris_common PRIVATE spdlog::spdlog nlohmann_json::nlohmann_json)
iris_set_warnings(iris_common)
