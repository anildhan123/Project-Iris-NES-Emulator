cmake_minimum_required(VERSION 3.25)

project(IrisEmulator
    VERSION 0.10.0
    DESCRIPTION "Iris Emulator"
    LANGUAGES C CXX
)

option(IRIS_DESKTOP "Build the SDL desktop application" ON)
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IRIS_DESKTOP OFF CACHE BOOL "Build the SDL desktop application" FORCE)
endif()

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
add_subdirectory(src/core)
if(IRIS_DESKTOP)
    add_subdirectory(src/frontend)
    add_subdirectory(src/app)
endif()
if(ANDROID)
    add_subdirectory(ports/android)
endif()
add_subdirectory(tests)

if(IRIS_DESKTOP)
    install(TARGETS iris RUNTIME DESTINATION .)
    install(FILES
        ${CMAKE_SOURCE_DIR}/README.md
        ${CMAKE_SOURCE_DIR}/LICENSE
        ${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md
        DESTINATION .
    )
    install(FILES $<TARGET_FILE_DIR:iris>/iris_demo.nes DESTINATION . OPTIONAL)

    set(CPACK_PACKAGE_NAME "IrisEmulator")
    set(CPACK_PACKAGE_VENDOR "Project Iris")
    set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
    set(CPACK_GENERATOR "ZIP")
    include(CPack)
endif()
