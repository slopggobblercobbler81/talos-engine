# Audio Subsystem (`AudioSystem`)

Technical documentation for the **Layer 2: Audio Subsystem** (`engine/audio/`, static library `talos_audio`) in **Talos Engine**.

---

## 1. Architectural Overview & Encapsulation

The Audio Subsystem resides in `engine/audio/` (`namespace Talos::Audio`) as an independent Layer 2 domain subsystem (`talos_audio`):

```
Editor -> Scripting -> Scene -> Audio (talos_audio) -> Core (talos_core)
```

* **Complete Backend Encapsulation:** `engine/audio/audio_system.h` exposes only engine-native types (`AudioFormat`, `SoundInfo`, `MusicInfo`, `AudioSystem`). `raylib.h` (`raudio` / `miniaudio`) is included strictly inside `engine/audio/audio_system.cpp` and linked `PRIVATE` to `talos_audio`.
* **Headless & Silent Fallback Safety:** If no hardware audio output device is available (e.g., headless CI runners or systems without an active sound card), `AudioSystem` automatically falls back to RAM wave decoding (`::LoadWave`) to validate files, extract accurate sample rate / frame count / duration metadata, and simulate playback progression without crashing.

---

## 2. Sound Effects vs. Music Streams

`AudioSystem` separates audio playback into two distinct models optimized for their memory and latency profiles:

| Model | Supported Formats | Storage Strategy | Typical Use Case |
| :--- | :--- | :--- | :--- |
| **`Sound`** | `.wav`, `.ogg` | Decoded **entirely in RAM** on load (`s_SoundCache`). Zero disk I/O during playback. | Short, low-latency sound effects (jumps, impacts, pickups, UI clicks). |
| **`Music`** | `.ogg`, `.mp3`, `.wav` | **Streamed in chunks** from disk (`s_MusicCache`, ~32 KB ring buffer per track) fed each frame by `AudioSystem::Update()`. | Long background music tracks and ambient loops. |

---

## 3. Lifecycle & Main Loop Integration

1. **Initialization (`AudioSystem::Init()`):**
   - Generates starter procedural `.wav` audio clips in `examples/assets/audio/` (`coin.wav`, `jump.wav`, `hit.wav`, `music_loop.wav`) if not already present.
   - Initializes the hardware audio device and applies the master volume (`s_MasterVolume`).
2. **Per-Frame Stream Update (`AudioSystem::Update()`):**
   - Called once per frame in the main loop (`editor/main.cpp`) to refill active `Music` stream buffers (`::UpdateMusicStream`).
3. **Play Mode Stop Reset (`Editor::StopPlayMode()`):**
   - Exiting Play Mode in the Editor automatically calls `AudioSystem::StopAllSounds()` and `AudioSystem::StopAllMusic()` so no audio bleeds back into Edit Mode.
4. **Deterministic Shutdown (`AudioSystem::Shutdown()`):**
   - Stops and unloads all cached `Sound` buffers and `Music` streams **before** closing the audio device (`::CloseAudioDevice()`).

---

## 4. Public API Summary (`engine/audio/audio_system.h`)

* **Device & Master Controls:**
  * `Init()`, `Shutdown()`, `IsInitialized()`, `IsDeviceReady()`, `Update()`
  * `SetMasterVolume(float volume)`, `GetMasterVolume()`
* **Format Detection:**
  * `DetectFormat(path)` $\rightarrow$ `AudioFormat::{Unknown, WAV, OGG, MP3}`
  * `IsSupportedSoundFormat(path)` (`WAV`, `OGG`)
  * `IsSupportedMusicFormat(path)` (`OGG`, `MP3`, `WAV`)
* **In-Memory `Sound` API:**
  * `LoadSound(path)`, `UnloadSound(path)`, `UnloadAllSounds()`, `HasSound(path)`
  * `PlaySound(path, volume, pitch, pan)`, `StopSound(path)`, `PauseSound(path)`, `ResumeSound(path)`, `StopAllSounds()`
  * `IsSoundPlaying(path)`, `IsSoundPaused(path)`
  * `SetSoundVolume`, `SetSoundPitch`, `SetSoundPan`, `GetSoundInfo(path)`
* **Streaming `Music` API:**
  * `LoadMusic(path)`, `UnloadMusic(path)`, `UnloadAllMusic()`, `HasMusic(path)`
  * `PlayMusic(path, loop, volume, pitch, pan)`, `StopMusic(path)`, `PauseMusic(path)`, `ResumeMusic(path)`, `StopAllMusic()`
  * `IsMusicPlaying(path)`, `IsMusicPaused(path)`, `SeekMusic(path, positionSeconds)`, `SetMusicLooping(path, loop)`, `IsMusicLooping(path)`
  * `SetMusicVolume`, `SetMusicPitch`, `SetMusicPan`, `GetMusicTimeLength(path)`, `GetMusicTimePlayed(path)`, `GetMusicInfo(path)`
* **Diagnostics & Telemetry:**
  * `GetCachedSoundCount()`, `GetCachedMusicCount()`, `GetActiveMusicStreamCount()`, `GetTotalSoundMemoryBytes()`, `GetTotalMusicMemoryBytes()`
