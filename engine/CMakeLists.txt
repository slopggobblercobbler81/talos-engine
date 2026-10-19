# Engine layers as static libraries. Link dependencies mirror the layering
# rules (docs/ARCHITECTURE.md): Core <- Renderer/Physics/Audio <- Scene.
# All layers share engine/ as include root (e.g. #include "core/window.h").

function(talos_add_layer name)
    add_library(${name} STATIC ${ARGN})
    target_include_directories(${name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

# Layer 1: Core
talos_add_layer(talos_core
    core/window.cpp
    core/input.cpp
    core/input_map.cpp
    core/logger.cpp
)
target_link_libraries(talos_core PRIVATE raylib)

# Layer 2: Renderer
talos_add_layer(talos_renderer renderer/renderer2d.cpp)
target_link_libraries(talos_renderer PUBLIC talos_core PRIVATE raylib)

# Layer 2: Physics
talos_add_layer(talos_physics physics/physics2d.cpp)

# Layer 2: Audio
talos_add_layer(talos_audio audio/audio_system.cpp)
target_link_libraries(talos_audio PUBLIC talos_core PRIVATE raylib)

# Layer 3: Scene
talos_add_layer(talos_scene
    scene/scene.cpp
    scene/scene_serializer.cpp
    scene/project_serializer.cpp
)
target_link_libraries(talos_scene PUBLIC talos_core talos_renderer talos_physics talos_audio)
