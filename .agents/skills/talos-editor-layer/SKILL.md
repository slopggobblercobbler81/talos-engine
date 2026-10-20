---
name: talos-editor-layer
description: >-
  Workflow, modular tree conventions, and UI/Undo rules for Layer 5 (Visual Editor)
  of Talos Engine (`editor/`). Use when modifying `EditorNode`, docks (`SceneTreeDock`,
  `InspectorDock`, `ConsoleDock`, `AssetBrowserDock`), `CanvasViewport`, `DiagnosticOverlay`,
  `EditorToolbar`, `EditorGui` widgets/inspectors, `EditorUndoRedo`, or modal dialogs.
---

# Talos Visual Editor Layer (`editor/`)

## 1. Layer Scope & Modular Tree Structure
- `editor/main.cpp` — Application bootstrap, window setup, starter assets, and main loop.
- `editor/editor.{h,cpp}` — Central orchestrator (`EditorNode`) coordinating branches:
  - `editor/docks/` — `scene_tree_dock`, `inspector_dock`, `console_dock`, `asset_browser_dock`.
  - `editor/gui/` — `editor_gui.{h,cpp}` (public declaration + core state), `editor_gui_widgets.cpp`, `editor_gui_text.cpp`, `editor_gui_sprite.cpp`, `editor_gui_components.cpp`.
  - `editor/viewport/` — `canvas_viewport.{h,cpp}` (camera pan/zoom, picking, gizmos) and `diagnostic_overlay.{h,cpp}` (`[F3]` HUD).
  - `editor/history/` — `editor_undo_redo.{h,cpp}` (snapshot Undo/Redo & continuous drag coalescing).
  - `editor/toolbar/` — `editor_toolbar.{h,cpp}` (Play/Stop & status notifications).
  - `editor/dialogs/` — `input_map_dialog.{h,cpp}`, `project_dialog.{h,cpp}`.

## 2. Strict Architectural Constraints
1. **Top Layer (Consumer Only):**
   - `editor/` consumes `engine/` layers using layer-relative includes (`#include "scene/..."`, `#include "renderer/..."`, `#include "core/..."`) and `#include "editor/..."` for editor files.
   - Engine layers (`engine/`) must **never** include anything from `editor/`.
2. **Complete Backend Encapsulation:**
   - Never include `raylib.h` in any `editor/` file.
   - All drawing, text measurement, and texture previews must go exclusively through `Renderer2D` (`engine/renderer/renderer2d.h`), and all input queries through `Input` / `InputMap` (`engine/core/`).
3. **Godot-Style Modular Tree (No Monolithic Files):**
   - Keep files focused. When an `editor/` file grows past ~400 lines or mixes unrelated responsibilities, split it cleanly by responsibility (like `editor/gui/editor_gui_*.cpp`).
   - Register any new `.cpp` file in the root `CMakeLists.txt` and start every new `.h` with a one-line `// ...` comment.
4. **Undo/Redo & Inspector Parity:**
   - Any new component property exposed in `InspectorDock` / `EditorGui` must also be compared in `AreEntityListsEqual` (`editor/history/editor_undo_redo.cpp`) and support continuous-drag undo coalescing.

## 3. Navigation & Workflow
1. Run `python tools/code_map.py editor` (or filter by branch, e.g., `python tools/code_map.py gui`) to find the exact files.
2. Search for symbols (`Grep`) before reading line ranges.
3. Update the matching `docs/` file (`docs/EDITOR_SYSTEM.md`, `docs/UNDO_REDO_SYSTEM.md`, `docs/TEXT_SELECTION_SYSTEM.md`, `docs/DEBUG_CONSOLE_SYSTEM.md`, `docs/DIAGNOSTIC_OVERLAY_SYSTEM.md`) when editor features change.

## 4. Verification
- Build the editor executable and run all unit tests from PowerShell:
  ```powershell
  cmake --build build
  ctest --test-dir build --output-on-failure
  ```
