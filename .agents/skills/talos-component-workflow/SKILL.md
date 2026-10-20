---
name: talos-component-workflow
description: >-
  Cross-layer checklist and fast navigation procedure for adding or modifying Entity
  and Component properties across Talos Engine (`Scene`, `SceneSerializer`, `EditorGui`,
  `EditorUndoRedo`, and unit tests). Use whenever a component struct or entity property
  is added, renamed, or modified.
---

# Talos Component Modification & Fast Navigation Workflow

## 1. Fast Navigation Procedure
1. **Route:** Run `python tools/code_map.py <keyword>` to identify the smallest set of files involved.
2. **Search Before Reading:** Search for an existing sibling field name across the repository to locate exact line numbers in every integration point.
3. **Read Ranges Only:** Read only the relevant struct declarations and function line ranges.

## 2. Mandatory 6-Point Component Modification Checklist
Whenever you add or modify an entity or component property, verify and update all 6 integration points:

1. **Data Structs (`engine/scene/components.h` / `engine/scene/entity.h`):**
   - Add the field with sensible default initialization in the flat struct.
2. **Runtime Execution (`engine/scene/scene.cpp`):**
   - Update `Scene::Update`, `Scene::Render`, or runtime helpers to apply the property during gameplay and editor preview.
3. **Inspector UI (`editor/docks/inspector_dock.cpp` & `editor/gui/editor_gui*.cpp`):**
   - Expose the control in the appropriate `EditorGui` component drawer (`editor_gui_sprite.cpp`, `editor_gui_components.cpp`, `editor_gui_text.cpp`, etc.) and wire it into `InspectorDock`.
4. **Serialization (`engine/scene/scene_serializer.cpp`):**
   - Serialize and deserialize the field in `.talsc` scenes and `.prefab` templates, including safe value clamping and backward-compatible default fallbacks.
5. **Undo/Redo History (`editor/history/editor_undo_redo.cpp`):**
   - Add the field comparison to `AreEntityListsEqual` so changes are tracked by Undo/Redo snapshots.
6. **Unit Tests (`tests/scene/test_scene_serializer.cpp`):**
   - Add the new field to the serialization round-trip and clamping unit tests.

## 3. Build & Test Verification
Always verify from PowerShell before finishing:
```powershell
cmake --build build --target test_scene_serializer
ctest --test-dir build -R test_scene_serializer --output-on-failure
cmake --build build
ctest --test-dir build --output-on-failure
```
