cmake_minimum_required(VERSION 3.25)

project(IrisEmulator
    VERSION 0.2.0
    DESCRIPTION "Iris Emulator"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CompilerWarnings.cmake)
include(cmake/Dependencies.cmake)

enable_testing()

add_subdirectory(src/common)
add_subdirectory(src/platform)
add_subdirectory(src/emulator)
add_subdirectory(src/systems/nes)
add_subdirectory(src/frontend)
add_subdirectory(src/app)
add_subdirectory(tests)
