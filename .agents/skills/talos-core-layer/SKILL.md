---
name: talos-core-layer
description: >-
  Workflow, architecture rules, and verification procedures for Layer 1 (Core & Platform)
  of Talos Engine (`engine/core/`, `talos_core`). Use when modifying window management,
  frame delta timing, keyboard/mouse input polling (`Input`), logical input actions
  (`InputMap`), or the classified debug logger (`Logger`).
---

# Talos Core & Platform Layer (`engine/core/`)

## 1. Layer Scope & Files
- `engine/core/window.{h,cpp}` — OS window lifecycle, display surface setup, target FPS, and frame timing (`delta`).
- `engine/core/input.{h,cpp}` — Engine-native key/mouse codes and hardware polling API (keyboard, mouse, wheel, typed characters).
- `engine/core/input_map.{h,cpp}` — Logical input actions (`InputMap`), key bindings, and `.talos` persistence.
- `engine/core/logger.{h,cpp}` — Classified debug logger (`INFO`, `WARN`, `ERROR`) with in-memory history and `TALOS_LOG_*` macros.
- Build target: `talos_core` static library in `engine/CMakeLists.txt`.

## 2. Strict Architectural Constraints
1. **Bottom Layer (Zero Upward Dependencies):**
   - `talos_core` is Layer 1. It must **never** include headers from or link against `renderer`, `physics`, `audio`, `scene`, `scripting`, or `editor`.
2. **Complete Backend Encapsulation (`raylib.h`):**
   - Never include `raylib.h` in any public header (`engine/core/*.h`).
   - Include `raylib.h` only inside `engine/core/*.cpp` implementation files.
   - Public headers must expose only Talos Engine types and key/mouse enums.
3. **Header & Code Conventions:**
   - Every `.h` file must start with a one-line `// ...` responsibility comment for `tools/code_map.py`.
   - Use layer-relative includes (`#include "core/..."`).

## 3. Navigation & Workflow
1. Run `python tools/code_map.py core` to inspect the current files in `engine/core/`.
2. Read the `.h` declaration before reading ranges in `.cpp`.
3. Consult `docs/CORE_SYSTEM.md` or `docs/INPUT_MAP_SYSTEM.md` only when changing behavior or public APIs, and update them if the architecture or features change.

## 4. Verification
- Compile the core library narrowly first:
  ```powershell
  cmake --build build --target talos_core
  ```
- Then run a full build and test pass from PowerShell:
  ```powershell
  cmake --build build
  ctest --test-dir build --output-on-failure
  ```
