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

# Engine layers (static libraries: talos_core, talos_renderer, talos_physics, talos_scene)
add_subdirectory(engine)

# Layer 5: Editor (Central Orchestrator + Specialized Branches)
add_executable(TalosEngine
    editor/editor.cpp
    editor/main.cpp
    editor/gui/editor_gui.cpp
    editor/gui/editor_gui_text.cpp
    editor/gui/editor_gui_widgets.cpp
    editor/gui/editor_gui_sprite.cpp
    editor/gui/editor_gui_components.cpp
    editor/history/editor_undo_redo.cpp
    editor/toolbar/editor_toolbar.cpp
    editor/viewport/canvas_viewport.cpp
    editor/viewport/diagnostic_overlay.cpp
    editor/docks/scene_tree_dock.cpp
    editor/docks/inspector_dock.cpp
    editor/docks/console_dock.cpp
    editor/dialogs/input_map_dialog.cpp
)

# Editor resolves "editor/..." from the repository root; engine headers come from the libraries
target_include_directories(TalosEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TalosEngine PRIVATE talos_scene talos_renderer talos_physics talos_core)

# Tests (disable with -DTALOS_BUILD_TESTS=OFF)
option(TALOS_BUILD_TESTS "Build engine unit tests" ON)
if(TALOS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
