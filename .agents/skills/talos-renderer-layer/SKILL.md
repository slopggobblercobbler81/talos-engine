---
name: talos-renderer-layer
description: >-
  Workflow, architecture rules, and rendering conventions for Layer 2 Renderer Subsystem
  of Talos Engine (`engine/renderer/`, `talos_renderer`). Use when modifying `Renderer2D`,
  draw calls, GPU texture caching, procedural checkerboard fallbacks, UV sub-rectangles,
  sprite flipping, sorting layers (`sortingLayer`, `zOrder`), typography, or `Camera2DData`.
---

# Talos Renderer Subsystem Layer (`engine/renderer/`)

## 1. Layer Scope & Files
- `engine/renderer/renderer2d.{h,cpp}` — `Renderer2D`: the exclusive 2D draw-call submission API, GPU texture cache, procedural fallback textures, UV sub-rectangle rendering, sorting layers, typography/font cache, and `Camera2DData` projection.
- Build target: `talos_renderer` static library in `engine/CMakeLists.txt`.

## 2. Strict Architectural Constraints
1. **Exclusive Drawing Boundary:**
   - `Renderer2D` is the **only** class in Talos Engine permitted to issue GPU or drawing calls. Every higher layer (`scene`, `editor`) must draw exclusively through `Renderer2D`.
2. **Strict Layering (Layer 2 -> Layer 1 Only):**
   - `talos_renderer` may depend **only** on `talos_core`.
   - It must **never** include or depend on `physics`, `audio`, `scene`, `scripting`, or `editor`.
3. **Complete Backend Encapsulation (`raylib.h`):**
   - Never include `raylib.h` in `engine/renderer/renderer2d.h`.
   - Keep `raylib.h` and GPU resource handles strictly encapsulated inside `engine/renderer/renderer2d.cpp` (`PRIVATE` link).

## 3. Navigation & Workflow
1. Run `python tools/code_map.py renderer` to locate renderer files.
2. Read `engine/renderer/renderer2d.h` first to understand the public drawing, texture, font, and camera APIs, then read only the target line ranges in `engine/renderer/renderer2d.cpp`.
3. Consult and update `docs/RENDERER_SYSTEM.md`, `docs/TEXTURE_SYSTEM.md`, or `docs/CAMERA_2D_SYSTEM.md` when changing rendering, texture caching, or 2D camera behavior.

## 4. Verification
- Compile `talos_renderer` narrowly while iterating:
  ```powershell
  cmake --build build --target talos_renderer
  ```
- Verify the full build and test suite before finishing:
  ```powershell
  cmake --build build
  ctest --test-dir build --output-on-failure
  ```
