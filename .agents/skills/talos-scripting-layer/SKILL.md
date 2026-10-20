---
name: talos-scripting-layer
description: >-
  Workflow, architecture rules, and sandboxing guidelines for Layer 4 (Scripting Layer)
  of Talos Engine (`engine/scripting/`, `third_party/lua/`, `talos_scripting`). Use when
  working on the Talos Lua VM fork, C++ script bindings, lifecycle hooks (`OnStart`,
  `OnUpdate`, `OnCollision`), VM sandboxing, instruction budgets, or live hot-reloading.
---

# Talos Scripting Layer (`engine/scripting/` & `third_party/lua/`)

## 1. Layer Scope & Files
- `engine/scripting/` — Scripting subsystem bridging compiled C++ engine subsystems with Talos Lua game scripts.
- `third_party/lua/` — Project-owned fork of Lua (MIT license), trimmed and adapted for Talos Engine.
- `third_party/lua/TALOS_CHANGES.md` — Mandatory changelog tracking every modification made to the vendored Lua source.
- `tests/scripting/` — Headless unit tests for Lua VM execution, bindings, sandboxing, and error reporting.

## 2. Strict Architectural Constraints
1. **Strict Layering (Layer 4 -> Layers 1, 2, 3 Only):**
   - `talos_scripting` may depend on `talos_scene`, Domain Subsystems (`talos_renderer`, `talos_physics`, `talos_audio`), and `talos_core`.
   - It must **never** include or depend on `editor/`.
2. **Complete VM Encapsulation:**
   - Lua headers (`lua.h`, `lauxlib.h`, `lualib.h`) must be included **only** inside `.cpp` files in `engine/scripting/`.
   - Public headers in `engine/scripting/*.h` must expose only Talos Engine types.
3. **Fork Discipline & Sandboxing:**
   - Unsafe standard libraries (`io`, `os`, `debug`, `package`) are stripped in the fork so scripts cannot access the host OS or filesystem.
   - Document every change to `third_party/lua/` in `third_party/lua/TALOS_CHANGES.md` and preserve the MIT copyright notice.
   - Enforce instruction budgets (infinite-loop protection) and report runtime/syntax errors with file and line numbers to `Logger` (`[ERROR]`) for the Debug Console.

## 3. Verification
- Build the project and run all unit tests from PowerShell:
  ```powershell
  cmake --build build
  ctest --test-dir build --output-on-failure
  ```
