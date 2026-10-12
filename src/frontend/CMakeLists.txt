add_library(iris_frontend STATIC
    iris/desktop_shell.cpp
    iris/desktop_shell.hpp
    iris/xp_theme.cpp
    iris/xp_theme.hpp
)

target_include_directories(iris_frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(iris_frontend PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(iris_frontend PUBLIC iris_common iris_platform)
target_link_libraries(iris_frontend PRIVATE SDL3::SDL3 imgui)
iris_set_warnings(iris_frontend)
