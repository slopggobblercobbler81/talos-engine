---
name: talos-scene-layer
description: >-
  Workflow, architecture rules, and serialization/testing procedures for Layer 3
  (Scene & Entity Layer) of Talos Engine (`engine/scene/`, `tests/scene/`, `talos_scene`).
  Use when working on `Entity`, flat components (`components.h`), `Scene` runtime update
  and rendering, `AssetManager`, `.talsc`/`.prefab` `SceneSerializer`, or `ProjectSerializer`.
---

# Talos Scene & Entity Layer (`engine/scene/`)

## 1. Layer Scope & Files
- `engine/scene/components.h` — Flat data component structs (`Transform`, `Sprite`, `Text`, `Collider`, `Movement`, `Camera`, `AudioSource`, `Script`, etc.).
- `engine/scene/entity.h` — Flat zero-inheritance `Entity` struct aggregating components.
- `engine/scene/scene.{h,cpp}` — `Scene` container: entity storage, deterministic per-frame update/render loop, runtime camera, and telemetry counters.
- `engine/scene/asset_manager.{h,cpp}` — `AssetManager`: central resource registry for loading, caching, reusing, and freeing textures, audio, fonts, prefabs, and scripts.
- `engine/scene/scene_serializer.{h,cpp}` — `SceneSerializer`: human-readable `.talsc` scene and `.prefab` template save/load.
- `engine/scene/project_serializer.{h,cpp}` — `ProjectSerializer`: unified `project.talos` configuration and project workflow.
- `tests/scene/test_scene.cpp`, `tests/scene/test_scene_serializer.cpp`, `tests/scene/test_project_serializer.cpp`, `tests/scene/test_asset_manager.cpp` — Headless unit tests.
- Build target: `talos_scene` in `engine/CMakeLists.txt`.

## 2. Strict Architectural Constraints
1. **Zero-Inheritance Entities:**
   - Keep `Entity` and all components in `components.h` and `entity.h` as flat data structs. Never introduce polymorphic class hierarchies or virtual methods for game objects.
2. **Strict Layering (Layer 3 -> Layers 1 & 2 Only):**
   - `talos_scene` depends on `talos_core`, `talos_renderer`, `talos_physics`, and `talos_audio`.
   - It must **never** include or depend on `scripting` or `editor`.
   - Never include `raylib.h` anywhere in `engine/scene/`. All rendering goes through `Renderer2D`, audio through `AudioSystem`, and physics through `Physics2D`.
3. **Component Modification Checklist:**
   - Whenever adding or modifying an entity/component field, apply the `talos-component-workflow` skill to keep `components.h`, `scene.cpp`, `scene_serializer.cpp`, `inspector_dock.cpp` / `editor_gui*.cpp`, `editor_undo_redo.cpp`, and `test_scene_serializer.cpp` in sync.

## 3. Navigation & Workflow
1. Run `python tools/code_map.py scene` to inspect scene and test files.
2. Search for symbol/field names before reading ranges in `scene.cpp` or `scene_serializer.cpp`.
3. Update `docs/SCENE_AND_ENTITIES.md`, `docs/SCENE_SERIALIZATION.md`, or `docs/ASSET_MANAGER_SYSTEM.md` when features change.

## 4. Verification
- Compile and run the scene unit tests from PowerShell:
  ```powershell
  cmake --build build --target test_scene_serializer test_project_serializer test_asset_manager
  ctest --test-dir build -R "test_scene_serializer|test_project_serializer|test_asset_manager" --output-on-failure
  ```
- Run the full build and test suite before finishing:
  ```powershell
  cmake --build build
  ctest --test-dir build --output-on-failure
  ```
