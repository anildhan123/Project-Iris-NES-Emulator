add_library(iris_frontend STATIC
    iris/desktop_shell.cpp
    iris/desktop_shell.hpp
    iris/picture_pass.cpp
    iris/picture_pass.hpp
    iris/shell_theme.cpp
    iris/shell_theme.hpp
)

target_include_directories(iris_frontend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(iris_frontend PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(iris_frontend PUBLIC iris_common iris_platform iris_emulator)
target_link_libraries(iris_frontend PRIVATE SDL3::SDL3 imgui)
if(WIN32)
    target_link_libraries(iris_frontend PRIVATE shell32 winmm)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(iris_frontend PRIVATE OpenGL::GL)
endif()
if(APPLE)
    target_compile_options(iris_frontend PRIVATE -Wno-deprecated-declarations)
endif()
iris_set_warnings(iris_frontend)
