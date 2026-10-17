cmake_minimum_required(VERSION 3.16)
project(TalosEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Download RayLib Automatically
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# List of modules in our layered engine
add_executable(TalosEngine
    core/window.cpp
    core/input.cpp
    core/logger.cpp
    renderer/renderer2d.cpp
    physics/physics2d.cpp
    scene/scene.cpp
    scene/scene_serializer.cpp
    editor/editor.cpp
    editor/main.cpp
)

# Allow for clean file inclusion based on layer (e.g., #include "core/window.h")
target_include_directories(TalosEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Connect Raylib internally
target_link_libraries(TalosEngine PRIVATE raylib)