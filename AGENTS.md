# AI Agent Guidelines (`AGENTS.md`)

Rules and documentation index for AI agents working on **Talos Engine**.

---

## 1. Mandatory Rules

1. **Read Only What You Need (Targeted Reading):**
   - Do **not** read every module or every doc before a change. Use [Code Discovery](#2-code-discovery) to find the files involved, then follow the [Fast Navigation Technique](#3-fast-navigation-technique).
   - Read a `docs/` file only when the task changes that system's behavior or architecture. Read [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) only for structural changes (new module, new layer, moved files).
   - Update the corresponding `docs/` file whenever architecture or features change.
2. **Strict Layered Dependencies (Top-Down Only):**
   - `Editor` $\rightarrow$ `Scripting` $\rightarrow$ `Scene` $\rightarrow$ `Subsystems (Renderer / Physics / Audio)` $\rightarrow$ `Core`.
   - Lower layers must **never** include or depend on higher layers.
   - Each engine layer is a static library in `engine/CMakeLists.txt` (`talos_core`, `talos_renderer`, `talos_physics`, `talos_audio`, `talos_scene`). Link dependencies must mirror the layering above.
3. **Complete Backend Encapsulation:**
   - Never include third-party headers (e.g., `raylib.h`) in public `.h` headers or outside the `.cpp` files of the layer that owns them: `engine/core/`, `engine/renderer/`, and `engine/audio/` for raylib, and `engine/scripting/` for the Lua VM once it exists. The Lua fork lives in `third_party/lua/`; document every modification of it in `third_party/lua/TALOS_CHANGES.md`.
   - raylib is linked `PRIVATE` only to `talos_core`, `talos_renderer`, and `talos_audio`. Do not link it anywhere else.
   - All drawing and GPU calls must go exclusively through `Renderer2D` (`engine/renderer/renderer2d.h`).
4. **No Monolithic Files (Godot-Style Modular Tree):**
   - Keep files small and focused. Use a central orchestrator per layer with specialized branch folders/files (e.g., `editor/editor.cpp` coordinating `docks/`, `viewport/`, `toolbar/`, `dialogs/`, `gui/`, `history/`).
   - Split a file by responsibility when it grows past roughly 400 lines or mixes unrelated concerns, as was done with `editor/gui/editor_gui*.cpp`. Do not split or rewrite working code without a clear benefit.
5. **Adding New Code (Structure Conventions):**
   - **One `.h` + one `.cpp` per major class** by default. Group small, closely related types in one file only when there is a clear reason. Never create files or classes just to follow a convention.
   - **Placement:** engine code goes in `engine/<module>/` (`core`, `renderer`, `physics`, `scene`, plus `audio` / `scripting` once they have real code). Editor code goes in `editor/<branch>/`. Do not create empty module folders in advance.
   - **First-line description:** every new `.h` (and every `.cpp` without a matching `.h`) must start with a one-line `// ...` comment describing its responsibility. `tools/code_map.py` builds the code map from it, so there is no separate map to maintain.
   - **Registration:** add new `.cpp` files to `engine/CMakeLists.txt` (engine) or the root `CMakeLists.txt` (editor), and tests to `tests/CMakeLists.txt`.
   - **Includes:** engine headers use the layer-relative style (`#include "core/window.h"`); editor headers use `#include "editor/..."`. Minimize cross-module includes, use forward declarations in headers when possible, and never create circular dependencies.
   - **Public APIs:** preserve existing public APIs, build configuration, and tests unless a change is necessary.
   - **Restructuring:** before any large reorganization, inspect the code and present the proposed changes to the user for approval.
6. **Zero-Inheritance Entities:**
   - Keep `Entity` and all components in `engine/scene/components.h` and `engine/scene/entity.h` as flat data structs. Never introduce polymorphic inheritance hierarchies for game objects.
7. **Component Modification Checklist:**
   Whenever you add or modify an entity/component property, you **must** update:
   - `engine/scene/components.h` / `engine/scene/entity.h` (Data struct)
   - `engine/scene/scene.cpp` (Runtime update/render logic)
   - `editor/docks/inspector_dock.cpp` & the matching `editor/gui/editor_gui*.cpp` file (Inspector UI)
   - `engine/scene/scene_serializer.cpp` (`.talos` save & load)
   - `editor/history/editor_undo_redo.cpp` (`AreEntityListsEqual` for Undo/Redo)
   - `tests/scene/test_scene_serializer.cpp` (add the field to the round-trip test)
8. **Tests:**
   - Add or update unit tests in `tests/` (mirrors `engine/`) for logic that needs no window or GPU (physics, serialization, math). Use the harness in `tests/test_framework.h`.
   - Run them with `ctest --test-dir build --output-on-failure`. On Windows run `ctest` from PowerShell (Git Bash resolves a different C++ runtime and fails with `0xc0000139`).
9. **Build, Language & Git Discipline:**
   - Communicate with the user in **Spanish**; write all C++ code, UI labels, comments, and commit messages in **English**.
   - Always verify builds with `cmake --build build` before finishing a task, and run the relevant tests. Report what was actually verified and what was not.
   - Keep commit messages short (one line). Do not add co-author or AI attribution lines.
   - Never run `git commit` or `git push` unless explicitly instructed by the user.

---

## 2. Code Discovery

There is no hand-written file map to maintain. Ask the code instead:

- `python tools/code_map.py` prints every source file with its line count and one-line description (about 40 rows).
- `python tools/code_map.py <keyword> [<keyword> ...]` filters it. All keywords must match the path or the description, e.g. `code_map.py gui slider` or `code_map.py serializ`.
- Folder names already encode the architecture: `engine/<layer>/` for runtime code, `editor/<branch>/` for tooling, `tests/` mirroring `engine/`. Files are named after their class (`physics2d.*`, `scene_serializer.*`), and large classes are split as `<class>_<responsibility>.cpp` (e.g., `editor_gui_sprite.cpp`).
- A file listed as `(no description ...)` is missing its first-line comment (rule 5). Add it when you touch that file.

---

## 3. Fast Navigation Technique

Use this procedure instead of exploring the codebase:

1. **Route:** run `python tools/code_map.py <keyword>` (see Code Discovery) to pick the smallest set of files. Do not read directories.
2. **Search before reading:** locate the symbol with a content search (e.g., `Grep` for `DrawCheckbox`, `restitution`, `AreEntityListsEqual`). Searching a field name across the repo shows every place it is used, which is the fastest way to apply the Component Modification Checklist.
3. **Read ranges, not files:** open only the function or block you will change (use the line numbers from the search). Read a whole file only if it is under ~150 lines or you are rewriting it.
4. **Headers first for APIs:** to understand how to call a module, read its `.h` (declarations and comments), not its `.cpp`.
5. **Batch independent calls:** run searches and reads that do not depend on each other in parallel in a single step.
6. **Build narrowly while iterating:** compile only what you changed (e.g., `cmake --build build --target test_physics2d`), then run the full `cmake --build build` once before finishing.
7. **Do not re-read** files you just edited or facts already established in the conversation.

---

## 4. Agent Policies

Policies that apply to every agent working on this repository. Each policy is a `###` subsection.

### Fast Execution Policy

- Treat Code Discovery (`tools/code_map.py`) as the default starting point, not as permission to scan the entire repository.
- Before opening files, identify the specific task category and the smallest relevant set of files.
- Search for symbols before reading source code. Read only the relevant declarations and implementation ranges.
- Expand the investigation only when a concrete dependency, compiler error, or test failure requires it.
- Apply checklists only when their triggering condition is met. For component changes, verify each listed integration point individually and modify only those that require changes.
- Do not repeat searches, reread unchanged files, or revalidate facts already established in the current task.
- Do not perform a full architecture review for routine changes.
- For a small task, keep the investigation proportional to the change. Correctness and required verification must still be preserved.
- If a task unexpectedly requires broad exploration, briefly state the reason before proceeding.

---

## 5. Documentation Index (`docs/`)

Consult only the entry matching the system you are changing.

### Architecture & Roadmap
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — Layered architecture, dependency rules, frame lifecycle, static libraries, tests, and directory tree structure.
- [`docs/ROADMAP_1_0.md`](docs/ROADMAP_1_0.md) — Roadmap to Talos Engine 1.0 (local file, not tracked in git).

### Layer 1: Core & Platform (`engine/core/`)
- [`docs/CORE_SYSTEM.md`](docs/CORE_SYSTEM.md) — Window lifecycle (`Window`), frame timing (`delta`), hardware input (`Input`), and logging (`Logger`).
- [`docs/INPUT_MAP_SYSTEM.md`](docs/INPUT_MAP_SYSTEM.md) — Logical input actions (`InputMap`), key bindings, and `.talos` input persistence.

### Layer 2: Domain Subsystems (`engine/renderer/`, `engine/physics/`, `engine/audio/`)
- [`docs/RENDERER_SYSTEM.md`](docs/RENDERER_SYSTEM.md) — 2D rendering pipeline (`Renderer2D`), sorting layers (`sortingLayer`, `zOrder`), UV sub-rectangles, and sprite flipping.
- [`docs/TEXTURE_SYSTEM.md`](docs/TEXTURE_SYSTEM.md) — GPU texture cache, lazy loading, starter assets, and procedural checkerboard fallback.
- [`docs/CAMERA_2D_SYSTEM.md`](docs/CAMERA_2D_SYSTEM.md) — 2D camera projection (`Camera2DData`), editor pan/zoom, and runtime active camera.
- [`docs/PHYSICS_SYSTEM.md`](docs/PHYSICS_SYSTEM.md) — 2D collision detection and resolution (`Physics2D`).
- [`docs/AUDIO_SYSTEM.md`](docs/AUDIO_SYSTEM.md) — Audio device lifecycle, in-memory `Sound` caching (WAV/OGG), and streaming `Music` playback (OGG/MP3/WAV) (`AudioSystem`).

### Layer 3: Scene & Entities (`engine/scene/`)
- [`docs/SCENE_AND_ENTITIES.md`](docs/SCENE_AND_ENTITIES.md) — Flat `Entity` and component data structures (`Transform`, `Sprite`, `Label`, `Collider2D`, `Camera`, `Script`) and `Scene` execution loop.
- [`docs/SCENE_SERIALIZATION.md`](docs/SCENE_SERIALIZATION.md) — Human-readable `.talos` scene serialization format (`SceneSerializer`).

### Layer 5: Visual Editor (`editor/`)
- [`docs/EDITOR_SYSTEM.md`](docs/EDITOR_SYSTEM.md) — Modular `Editor` orchestrator, docks (`SceneTreeDock`, `InspectorDock`, `ConsoleDock`), toolbar, and viewport.
- [`docs/UNDO_REDO_SYSTEM.md`](docs/UNDO_REDO_SYSTEM.md) — Snapshot-based Undo/Redo history (`EditorUndoRedo`) and continuous drag coalescing.
- [`docs/TEXT_SELECTION_SYSTEM.md`](docs/TEXT_SELECTION_SYSTEM.md) — Interactive text field controller (`TextEditState`: caret, selection, clipboard, auto-scroll).
- [`docs/DEBUG_CONSOLE_SYSTEM.md`](docs/DEBUG_CONSOLE_SYSTEM.md) — Real-time debug console dock (`ConsoleDock`) with severity filters.
- [`docs/DIAGNOSTIC_OVERLAY_SYSTEM.md`](docs/DIAGNOSTIC_OVERLAY_SYSTEM.md) — Toggleable `[F3]` viewport diagnostic HUD (`DiagnosticOverlay`) and telemetry counters.
