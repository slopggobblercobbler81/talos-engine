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

# List of modules in our layered engine (Godot-style modular tree)
add_executable(TalosEngine
    # Layer 1: Core
    engine/core/window.cpp
    engine/core/input.cpp
    engine/core/input_map.cpp
    engine/core/logger.cpp

    # Layer 2: Renderer & Physics
    engine/renderer/renderer2d.cpp
    engine/physics/physics2d.cpp

    # Layer 3: Scene
    engine/scene/scene.cpp
    engine/scene/scene_serializer.cpp

    # Layer 5: Editor (Central Orchestrator + Specialized Branches)
    editor/editor.cpp
    editor/main.cpp
    editor/gui/editor_gui.cpp
    editor/history/editor_undo_redo.cpp
    editor/toolbar/editor_toolbar.cpp
    editor/viewport/canvas_viewport.cpp
    editor/viewport/diagnostic_overlay.cpp
    editor/docks/scene_tree_dock.cpp
    editor/docks/inspector_dock.cpp
    editor/docks/console_dock.cpp
    editor/dialogs/input_map_dialog.cpp
)

# Allow for clean file inclusion based on layer (e.g., #include "core/window.h"
# resolves to engine/core/window.h, #include "editor/editor.h" to editor/editor.h)
target_include_directories(TalosEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
)

# Connect Raylib internally
target_link_libraries(TalosEngine PRIVATE raylib)