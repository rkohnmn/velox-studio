cmake_minimum_required(VERSION 3.16)
project(velox-studio
    VERSION 2.0.0
    DESCRIPTION "Bundled Native C++20 Data, Schema, Profiler & ImGui/Photon Vector Workbench"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTING "Build unit tests" ON)
option(ENABLE_SANITIZERS "Enable ASan/UBSan in debug builds" OFF)

add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter
    >
    $<$<CXX_COMPILER_ID:MSVC>:
        /W4 /WX /permissive- /wd4324 /wd4996
    >
)

add_library(project_sanitizers INTERFACE)
if(ENABLE_SANITIZERS AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(project_sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(project_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

set(VELOX_CORE_SOURCES
    src/arena.cpp
    src/sha256.cpp
    src/csv_engine.cpp
    src/viz_engine.cpp
    src/schema_engine.cpp
    src/profiler_engine.cpp
    src/provenance_engine.cpp
    src/studio_ui.cpp
    src/native_gui.cpp
    src/ux_research_engine.cpp
)

add_library(velox_core STATIC ${VELOX_CORE_SOURCES})
target_include_directories(velox_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(velox_core PRIVATE project_warnings project_sanitizers)

if(WIN32)
    target_compile_definitions(velox_core PUBLIC PLATFORM_WINDOWS _CRT_SECURE_NO_WARNINGS NOMINMAX)
elseif(APPLE)
    target_compile_definitions(velox_core PUBLIC PLATFORM_MACOS)
else()
    target_compile_definitions(velox_core PUBLIC PLATFORM_LINUX)
endif()

add_executable(velox src/main.cpp)
if(WIN32)
    target_link_libraries(velox PRIVATE velox_core project_warnings gdi32 user32 comdlg32 shell32)
else()
    target_link_libraries(velox PRIVATE velox_core project_warnings pthread ${CMAKE_DL_LIBS})
endif()

if(BUILD_TESTING)
    enable_testing()
    add_executable(velox_tests tests/test_main.cpp)
    if(WIN32)
        target_link_libraries(velox_tests PRIVATE velox_core project_warnings gdi32 user32 comdlg32 shell32)
    else()
        target_link_libraries(velox_tests PRIVATE velox_core project_warnings pthread ${CMAKE_DL_LIBS})
    endif()
    add_test(NAME VeloxUnitSuite COMMAND velox_tests)
endif()
