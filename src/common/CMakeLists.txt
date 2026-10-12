add_library(iris_common STATIC
    iris/byte_io.cpp
    iris/byte_io.hpp
    iris/config.cpp
    iris/config.hpp
    iris/crash_report.cpp
    iris/crash_report.hpp
    iris/error.hpp
    iris/input_bindings.hpp
    iris/library.cpp
    iris/library.hpp
    iris/log.cpp
    iris/log.hpp
    iris/sha256.cpp
    iris/sha256.hpp
    iris/version.hpp
)

target_include_directories(iris_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iris_common PRIVATE spdlog::spdlog nlohmann_json::nlohmann_json)
iris_set_warnings(iris_common)
