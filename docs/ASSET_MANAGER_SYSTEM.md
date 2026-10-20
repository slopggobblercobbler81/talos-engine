# Central Resource Manager (`AssetManager`)

`AssetManager` (`engine/scene/asset_manager.h`, `engine/scene/asset_manager.cpp`) provides a unified registry for loading, reusing, inspecting, and unloading all engine assets across subsystems while respecting Talos Engine's top-down layered architecture (`Scene` $\rightarrow$ `Renderer` / `Audio` / `Core`).

---

## 1. Supported Resource Categories (`AssetType`)

| Category | Extensions | Backing Storage / Subsystem |
| :--- | :--- | :--- |
| `AssetType::Texture` | `.png`, `.bmp`, `.tga`, `.jpg`, `.jpeg`, `.gif`, `.qoi` | Delegated to `Renderer2D` GPU texture cache (`LoadTexture`, `UnloadTexture`, `UnloadAllTextures`) |
| `AssetType::Sound` | `.wav`, `.ogg` | Delegated to `AudioSystem` in-memory sound cache (`LoadSound`, `UnloadSound`, `UnloadAllSounds`) |
| `AssetType::Music` | `.mp3`, `.flac`, `.xm`, `.mod` (or explicit `LoadMusic` on `.ogg`/`.wav`) | Delegated to `AudioSystem` streaming music registry (`LoadMusic`, `UnloadMusic`, `UnloadAllMusic`) |
| `AssetType::Font` | `.ttf`, `.otf`, `.fnt` | Delegated to `Renderer2D` GPU font cache (`LoadFont`, `UnloadFont`, `UnloadAllFonts`) |
| `AssetType::Prefab` | `.prefab`, `.talprefab` | In-memory `Entity` template cache integrated with `SceneSerializer` (`LoadPrefab`, `SavePrefab`, `InstantiatePrefab`) |
| `AssetType::Script` | `.lua` | In-memory script source cache (`ScriptAsset`: normalized path, source text, byte size) |

---

## 2. Path Normalization & Cache Reuse

- **`AssetManager::NormalizePath(rawPath)`**: Trims whitespace, converts backslashes (`\`) to forward slashes (`/`), collapses duplicate slashes, and strips leading `./` prefixes so equivalent paths always resolve to a single cache key.
- **Registry Metadata (`AssetRecord`)**: Every registered asset tracks:
  - `path`: Normalized project-relative path.
  - `type`: `AssetType` classification.
  - `loaded`: Current residency state.
  - `loadCount`: Number of times the resource was loaded from disk or generated.
  - `reuseCount`: Number of cache hits where an already-loaded resource was reused without disk I/O.
  - `estimatedBytes`: Approximate memory / VRAM footprint.

---

## 3. Core API Overview

### Lifecycle & Generic Dispatch
- `AssetManager::Init()` / `AssetManager::Shutdown()`: Initializes or clears all cached prefabs, scripts, fonts, textures, sounds, and music streams.
- `AssetManager::DetectType(path)`: Classifies a file path by its lowercase extension.
- `AssetManager::Load(path)` / `AssetManager::IsLoaded(path)` / `AssetManager::Unload(path)` / `AssetManager::Reload(path)`: Generic path-based operations that automatically dispatch to the corresponding category handler.

### Category-Specific API
- **Textures:** `LoadTexture(path)`, `IsTextureLoaded(path)`, `UnloadTexture(path)`, `UnloadAllTextures()`.
- **Sounds:** `LoadSound(path)`, `IsSoundLoaded(path)`, `UnloadSound(path)`, `UnloadAllSounds()`.
- **Music:** `LoadMusic(path)`, `IsMusicLoaded(path)`, `UnloadMusic(path)`, `UnloadAllMusic()`.
- **Fonts:** `LoadFont(path, baseSize)`, `IsFontLoaded(path)`, `UnloadFont(path)`, `UnloadAllFonts()`.
- **Prefabs:** `LoadPrefab(path, outPrefab)`, `GetPrefab(path)`, `CachePrefab(path, prefabEntity)`, `IsPrefabLoaded(path)`, `UnloadPrefab(path)`, `UnloadAllPrefabs()`.
- **Scripts:** `LoadScript(path)`, `GetScriptSource(path)`, `GetScript(path)`, `IsScriptLoaded(path)`, `UnloadScript(path)`, `UnloadAllScripts()`.

### Scene-Aware Lifecycle Management
- `AssetManager::PreloadSceneAssets(scene)`: Scans all entities in a `Scene` and preloads referenced `Sprite::texturePath`, `AudioSource` (`Sound` or `Music`), and `Script::scriptPath` resources.
- `AssetManager::UnloadUnusedSceneAssets(scene)`: Releases cached textures, sounds, music streams, and scripts that are not referenced by any entity in the active `Scene`.
- `AssetManager::GetStats()`: Returns an `AssetStats` snapshot with per-category counts, `totalLoaded`, `totalCacheHits`, and `totalEstimatedBytes`.
