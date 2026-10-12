include(FetchContent)

# Catch2 builds its own tests when BUILD_TESTING is on. Iris registers its
# tests with enable_testing() directly.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL_TESTS OFF CACHE BOOL "" FORCE)

set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)

set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS ON CACHE BOOL "" FORCE)

# Tags confirmed against the upstream repositories on 2026-10-12.
FetchContent_Declare(
    SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.18
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9b
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
    GIT_SHALLOW TRUE
)

if(IRIS_DESKTOP)
    FetchContent_MakeAvailable(SDL imgui spdlog nlohmann_json Catch2)
else()
    FetchContent_MakeAvailable(spdlog nlohmann_json Catch2)
endif()

if(IRIS_DESKTOP)
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    )

    target_include_directories(imgui SYSTEM PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )

    target_link_libraries(imgui PUBLIC SDL3::SDL3)
    target_compile_definitions(imgui PRIVATE SDL_MAIN_HANDLED)

    if(WIN32)
        target_link_libraries(imgui PUBLIC opengl32)
    endif()

    if(MSVC)
        target_compile_options(imgui PRIVATE /W0)
    else()
        target_compile_options(imgui PRIVATE -w)
    endif()
endif()

if(WIN32)
    # Log files use wide paths so non-ASCII application-data directories open correctly.
    target_compile_definitions(spdlog PUBLIC SPDLOG_WCHAR_FILENAMES)
endif()

if(IRIS_DESKTOP)
    message(STATUS "Iris dependencies: SDL 3.4.18, Dear ImGui 1.92.9b, spdlog 1.17.0, nlohmann/json 3.12.0, Catch2 3.16.1")
else()
    message(STATUS "Iris core dependencies: spdlog 1.17.0, nlohmann/json 3.12.0, Catch2 3.16.1")
endif()
