# Contributing to Talos Engine

Thank you for your interest in contributing to **Talos Engine**! This document covers how to set up your environment, navigate the codebase, follow our architectural rules, and submit a pull request.

---

## 1. Finding Something to Work On

Browse the [GitHub Issues](https://github.com/Elabsurdo984/talos-engine/issues) page to find open tasks. Issues are organized with custom labels:

- **Starter tasks:** `good first issue` — Self-contained fixes ideal for first-time contributors.
- **Issue types:**
  - `bug` — Defects, edge cases, and behavioral fixes.
  - `type:refactor` — Modularization, code structure, and architectural cleanup.
  - `type:docs` — Architecture and subsystem documentation (`docs/`).
  - `type:tests` — Unit tests and test harness coverage (`tests/`).
  - `type:idea` — New feature ideas, proposals, and workflow suggestions for the engine.
- **Engine layers:** `layer:core`, `layer:renderer`, `layer:physics`, `layer:audio`, `layer:scene`, `layer:scripting`, `layer:editor`.

If you plan to work on an existing issue, leave a brief comment on the ticket so others know it is in progress.

### Proposing New Ideas
**Anyone is completely welcome to propose new ideas for Talos Engine!** If you have an idea for a new feature, editor workflow improvement, subsystem enhancement, or tooling addition, feel free to [open a new issue](https://github.com/Elabsurdo984/talos-engine/issues/new) describing:
- What problem or workflow the idea addresses.
- How it fits into Talos Engine's lightweight, flat-entity philosophy and layered architecture.
- Any initial thoughts on its design or scope before starting implementation.

---

## 2. Development Setup, Build & Tests

### Prerequisites
- **C++17** compatible compiler (MSVC, GCC, or Clang)
- **CMake 3.16+**
- **Python 3** (optional, used by `tools/code_map.py` for fast codebase navigation)

### Configure & Build
From the repository root:

```bash
cmake -B build
cmake --build build
```

When iterating on a single test target, you can compile just that target to save time:

```bash
cmake --build build --target test_scene
```

Always run a full `cmake --build build` before submitting your changes.

### Running Unit Tests
Unit tests live in `tests/` (mirroring `engine/`) and use the lightweight harness in `tests/test_framework.h`. Run the entire test suite with CTest:

```powershell
ctest --test-dir build --output-on-failure
```

> **Windows Note:** Always run `ctest` from **PowerShell** or **Command Prompt**. Running test executables from Git Bash may resolve a conflicting C++ runtime DLL (`libstdc++-6.dll`) and fail with exit code `0xc0000139`.

---

## 3. Navigating the Codebase

Instead of manually browsing every folder, use the built-in code map tool to locate files and responsibilities:

```bash
# List all source files with line counts and one-line descriptions
python tools/code_map.py

# Filter by keywords (all keywords must match path or description)
python tools/code_map.py scene serializ
python tools/code_map.py gui slider
```

For deeper technical context on a specific subsystem, read the corresponding guide in [`docs/`](docs/) (starting with [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for structural changes).

---

## 4. Architecture & Coding Rules

Talos Engine enforces strict architectural boundaries to stay fast, modular, and maintainable:

### Strict Top-Down Layered Dependencies
```text
Editor (Layer 5)
  └── Scripting (Layer 4: talos_scripting)
        └── Scene (Layer 3: talos_scene)
              ├── Renderer (Layer 2: talos_renderer)
              ├── Physics  (Layer 2: talos_physics)
              └── Audio    (Layer 2: talos_audio)
                    └── Core (Layer 1: talos_core)
```
- **Top-down only:** Lower layers must **never** `#include` or link against higher layers.
- **Include style:** Engine headers use layer-relative paths (`#include "core/window.h"`, `#include "scene/scene.h"`); editor headers use `#include "editor/..."`.

### Complete Backend Encapsulation
- Never `#include` third-party headers (such as `raylib.h`) in public `.h` headers or outside the `.cpp` files of the layer that owns them (`engine/core/`, `engine/renderer/`, and `engine/audio/`).
- `raylib` is linked `PRIVATE` only to `talos_core`, `talos_renderer`, and `talos_audio`.
- All rendering and GPU calls must go exclusively through `Renderer2D` (`engine/renderer/renderer2d.h`).

### Zero-Inheritance Entities
- Keep `Entity` and all component types in `engine/scene/components.h` and `engine/scene/entity.h` as flat data structs. Do not introduce polymorphic inheritance hierarchies for game objects.

### Modular Files & Conventions
- **Keep files focused:** Split `.cpp` files by responsibility when they grow past roughly 400 lines or mix unrelated concerns (e.g., `editor/gui/editor_gui_*.cpp`).
- **First-line description:** Every new `.h` file (and every `.cpp` file without a matching `.h`) **must** begin on line 1 with a single-line `// ...` comment describing its responsibility so `python tools/code_map.py` can index it.
- **CMake registration:** Register new engine `.cpp` files in `engine/CMakeLists.txt`, editor `.cpp` files in the root `CMakeLists.txt`, and unit tests in `tests/CMakeLists.txt`.

---

## 5. Component Modification Checklist

Whenever you add, rename, or modify a property on `Entity` or any component struct, verify and update **all** of the following integration points:

1. `engine/scene/components.h` / `engine/scene/entity.h` — Data struct definition and default value.
2. `engine/scene/scene.cpp` — Runtime update and/or render logic.
3. `editor/docks/inspector_dock.cpp` & `editor/gui/editor_gui*.cpp` — Inspector UI widgets.
4. `engine/scene/scene_serializer.cpp` — `.talsc` / `.prefab` serialization and deserialization.
5. `editor/history/editor_undo_redo.cpp` — `AreEntityListsEqual` comparison for Undo/Redo change detection.
6. `tests/scene/test_scene_serializer.cpp` — Round-trip serialization unit test.

---

## 6. Submitting a Pull Request

1. **Create a focused branch** from `master` for your fix or improvement (one logical issue per PR).
2. **Write all code and documentation in English:** All C++ identifiers, UI labels, comments, documentation, commit messages, and pull request descriptions must be written in English.
3. **Add or update unit tests** in `tests/` whenever modifying core, physics, audio, or scene logic that does not require an OS window or GPU context.
4. **Update documentation** in `docs/` if your change modifies a subsystem's behavior, file format, or architecture.
5. **Verify before opening the PR:**
   - `cmake --build build` compiles cleanly with zero errors.
   - `ctest --test-dir build --output-on-failure` passes 100% of unit tests.
6. **Commit messages:** Keep commit messages concise (a single line in imperative mood, e.g., `Enforce MAX_UNDO_STEPS bound in EditorUndoRedo::Undo and Redo`).
