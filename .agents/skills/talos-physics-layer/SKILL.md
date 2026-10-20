---
name: talos-physics-layer
description: >-
  Workflow, architecture rules, and unit testing procedures for Layer 2 Physics Subsystem
  of Talos Engine (`engine/physics/`, `tests/physics/`, `talos_physics`). Use when working
  on `Physics2D`, 2D shapes (AABB, Circle), overlap detection, collision manifolds,
  impulse/restitution resolution, or `test_physics2d`.
---

# Talos Physics Subsystem Layer (`engine/physics/`)

## 1. Layer Scope & Files
- `engine/physics/physics2d.{h,cpp}` — `Physics2D`: 2D collision primitives (AABB, Circle), overlap tests, contact manifold generation (normal, penetration depth), and impulse/restitution resolution.
- `tests/physics/test_physics2d.cpp` — Headless mathematical unit tests for `Physics2D` using `tests/test_framework.h`.
- Build targets: `talos_physics` in `engine/CMakeLists.txt` and `test_physics2d` in `tests/CMakeLists.txt`.

## 2. Strict Architectural Constraints
1. **Pure Mathematical Domain Subsystem:**
   - `talos_physics` does **not** link `raylib` and requires no window or GPU context.
   - It may depend at most on `talos_core` (or standard C++ math headers) and must **never** include or depend on `renderer`, `audio`, `scene`, `scripting`, or `editor`.
2. **Deterministic & Testable:**
   - Every new collision shape, overlap query, manifold calculation, or resolution rule in `Physics2D` must be covered by unit tests in `tests/physics/test_physics2d.cpp`.

## 3. Navigation & Workflow
1. Run `python tools/code_map.py physics` to inspect the physics module and test files.
2. Read `engine/physics/physics2d.h` first, then target only the relevant functions in `engine/physics/physics2d.cpp`.
3. Update `docs/PHYSICS_SYSTEM.md` whenever collision detection or resolution behavior changes.

## 4. Verification
- Compile and run `test_physics2d` from PowerShell while iterating:
  ```powershell
  cmake --build build --target test_physics2d
  ctest --test-dir build -R test_physics2d --output-on-failure
  ```
- Run the full build and test suite before finishing:
  ```powershell
  cmake --build build
  ctest --test-dir build --output-on-failure
  ```
