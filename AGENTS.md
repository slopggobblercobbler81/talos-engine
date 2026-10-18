# AI Agent Guidelines (`AGENTS.md`)

Rules and documentation index for AI agents working on **Talos Engine**.

---

## 1. Mandatory Rules

1. **Read Docs Before Coding:** Always consult [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) and the relevant system document in `docs/` before modifying any module. Update the corresponding `docs/` files whenever architecture or features change.
2. **Strict Layered Dependencies (Top-Down Only):**
   - `Editor` $\rightarrow$ `Scripting` $\rightarrow$ `Scene` $\rightarrow$ `Subsystems (Renderer / Physics)` $\rightarrow$ `Core`.
   - Lower layers must **never** include or depend on higher layers.
3. **Complete Backend Encapsulation:**
   - Never include third-party headers (e.g., `raylib.h`) in public `.h` headers or outside `core/` and `renderer/` `.cpp` files.
   - All drawing and GPU calls must go exclusively through `Renderer2D` (`renderer/renderer2d.h`).
4. **No Monolithic Files (Godot-Style Modular Tree):**
   - Keep files small and focused. Use a central orchestrator per layer with specialized branch folders/files (e.g., `editor/editor.cpp` coordinating `docks/`, `viewport/`, `toolbar/`, `dialogs/`, `gui/`, `history/`).
5. **Zero-Inheritance Entities:**
   - Keep `Entity` and all components in `scene/components.h` and `scene/entity.h` as flat data structs. Never introduce polymorphic inheritance hierarchies for game objects.
6. **Component Modification Checklist:**
   Whenever you add or modify an entity/component property, you **must** update:
   - `scene/components.h` / `scene/entity.h` (Data struct)
   - `scene/scene.cpp` (Runtime update/render logic)
   - `editor/docks/inspector_dock.cpp` & `editor/gui/editor_gui.cpp` (Inspector UI)
   - `scene/scene_serializer.cpp` (`.talos` save & load)
   - `editor/history/editor_undo_redo.cpp` (`AreEntityListsEqual` for Undo/Redo)
7. **Build, Language & Git Discipline:**
   - Communicate with the user in **Spanish**; write all C++ code, UI labels, comments, and commit messages in **English**.
   - Always verify builds with `cmake --build build` before finishing a task.
   - Never run `git commit` or `git push` unless explicitly instructed by the user.

---

## 2. Documentation Index (`docs/`)

### Architecture & Roadmap
- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — Layered architecture, dependency rules, frame lifecycle, and directory tree structure.
- [`docs/ROADMAP_1_0.md`](docs/ROADMAP_1_0.md) — Roadmap to Talos Engine 1.0 (completed milestones and pending features).

### Layer 1: Core & Platform (`core/`)
- [`docs/CORE_SYSTEM.md`](docs/CORE_SYSTEM.md) — Window lifecycle (`Window`), frame timing (`delta`), hardware input (`Input`), and logging (`Logger`).
- [`docs/INPUT_MAP_SYSTEM.md`](docs/INPUT_MAP_SYSTEM.md) — Logical input actions (`InputMap`), key bindings, and `.talos` input persistence.

### Layer 2: Domain Subsystems (`renderer/`, `physics/`)
- [`docs/RENDERER_SYSTEM.md`](docs/RENDERER_SYSTEM.md) — 2D rendering pipeline (`Renderer2D`), sorting layers (`sortingLayer`, `zOrder`), UV sub-rectangles, and sprite flipping.
- [`docs/TEXTURE_SYSTEM.md`](docs/TEXTURE_SYSTEM.md) — GPU texture cache, lazy loading, starter assets, and procedural checkerboard fallback.
- [`docs/CAMERA_2D_SYSTEM.md`](docs/CAMERA_2D_SYSTEM.md) — 2D camera projection (`Camera2DData`), editor pan/zoom, and runtime active camera.
- [`docs/PHYSICS_SYSTEM.md`](docs/PHYSICS_SYSTEM.md) — 2D AABB overlap detection and axis-separated slide collision resolution (`Physics2D`).

### Layer 3: Scene & Entities (`scene/`)
- [`docs/SCENE_AND_ENTITIES.md`](docs/SCENE_AND_ENTITIES.md) — Flat `Entity` and component data structures (`Transform`, `Sprite`, `Label`, `Collider2D`, `Camera`, `Script`) and `Scene` execution loop.
- [`docs/SCENE_SERIALIZATION.md`](docs/SCENE_SERIALIZATION.md) — Human-readable `.talos` scene serialization format (`SceneSerializer`).

### Layer 5: Visual Editor (`editor/`)
- [`docs/EDITOR_SYSTEM.md`](docs/EDITOR_SYSTEM.md) — Modular `Editor` orchestrator, docks (`SceneTreeDock`, `InspectorDock`, `ConsoleDock`), toolbar, and viewport.
- [`docs/UNDO_REDO_SYSTEM.md`](docs/UNDO_REDO_SYSTEM.md) — Snapshot-based Undo/Redo history (`EditorUndoRedo`) and continuous drag coalescing.
- [`docs/TEXT_SELECTION_SYSTEM.md`](docs/TEXT_SELECTION_SYSTEM.md) — Interactive text field controller (`TextEditState`: caret, selection, clipboard, auto-scroll).
- [`docs/DEBUG_CONSOLE_SYSTEM.md`](docs/DEBUG_CONSOLE_SYSTEM.md) — Real-time debug console dock (`ConsoleDock`) with severity filters.
- [`docs/DIAGNOSTIC_OVERLAY_SYSTEM.md`](docs/DIAGNOSTIC_OVERLAY_SYSTEM.md) — Toggleable `[F3]` viewport diagnostic HUD (`DiagnosticOverlay`) and telemetry counters.
