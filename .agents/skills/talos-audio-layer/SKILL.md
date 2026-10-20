---
name: talos-audio-layer
description: >-
  Workflow, architecture rules, and unit test procedures for Layer 2 Audio Subsystem
  of Talos Engine (`engine/audio/`, `tests/audio/`, `talos_audio`). Use when working on
  `AudioSystem`, audio device lifecycle, in-memory WAV/OGG `Sound` caching, streaming
  OGG/MP3/WAV `Music` playback, volume/pitch/pan controls, or `test_audio_system`.
---

# Talos Audio Subsystem Layer (`engine/audio/`)

## 1. Layer Scope & Files
- `engine/audio/audio_system.{h,cpp}` — `AudioSystem` managing audio device initialization/shutdown, master volume, in-memory `Sound` effect caching (WAV/OGG), and streaming `Music` playback (OGG/MP3/WAV) with volume, pitch, pan, and loop controls.
- `tests/audio/test_audio_system.cpp` — Headless unit tests for `AudioSystem` using `tests/test_framework.h`.
- Build targets: `talos_audio` in `engine/CMakeLists.txt` and `test_audio_system` in `tests/CMakeLists.txt`.

## 2. Strict Architectural Constraints
1. **Strict Layering (Layer 2 -> Layer 1 Only):**
   - `talos_audio` may depend **only** on `talos_core` (e.g., `core/logger.h`).
   - It must **never** include or depend on `renderer`, `physics`, `scene`, `scripting`, or `editor`.
2. **Complete Backend Encapsulation (`raylib.h`):**
   - Never include `raylib.h` or expose raylib audio types (`Sound`, `Music`, `AudioStream`) in `engine/audio/audio_system.h`.
   - Keep `raylib.h` strictly inside `engine/audio/audio_system.cpp` (`talos_audio` links `raylib` as `PRIVATE`).
3. **Headless / Graceful Device Fallback:**
   - All `AudioSystem` APIs must behave deterministically and safely even when no physical audio device is available or initialized (critical for CI and unit tests in `test_audio_system`).

## 3. Navigation & Workflow
1. Run `python tools/code_map.py audio` to locate audio files.
2. Read `engine/audio/audio_system.h` first, then search for specific symbols before reading ranges in `engine/audio/audio_system.cpp`.
3. Update `docs/AUDIO_SYSTEM.md` whenever audio features or public APIs change.

## 4. Verification
- Build and run the audio unit test target while iterating:
  ```powershell
  cmake --build build --target test_audio_system
  ctest --test-dir build -R test_audio_system --output-on-failure
  ```
- Verify the full build before finishing:
  ```powershell
  cmake --build build
  ctest --test-dir build --output-on-failure
  ```
