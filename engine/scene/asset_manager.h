// Central resource manager for loading, caching, reusing, and freeing engine assets
#pragma once

#include "audio/audio_system.h"
#include "scene/entity.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#ifdef PlaySound
#undef PlaySound
#endif

namespace Talos::Scene {

    class Scene;

    enum class AssetType : std::uint8_t {
        Unknown = 0,
        Texture, // .png, .bmp, .jpg, .jpeg (via Renderer2D)
        Sound,   // .wav, .ogg in-memory SFX (via AudioSystem)
        Music,   // .ogg, .mp3, .wav streaming audio (via AudioSystem)
        Font,    // .ttf, .otf typography atlases (via Renderer2D)
        Prefab,  // .prefab reusable Entity templates (via SceneSerializer)
        Script   // .lua source text & metadata
    };

    struct ScriptAsset {
        std::string path;
        std::string source;
        std::size_t lineCount = 0;
        std::size_t byteSize = 0;
    };

    struct AssetRecord {
        std::string path;
        AssetType type = AssetType::Unknown;
        bool loaded = false;
        std::size_t memoryBytes = 0;
        std::uint32_t loadCount = 0;  // Total Load*() calls for this resource
        std::uint32_t reuseCount = 0; // Cache hits after initial load (loadCount - 1 when reused)
    };

    struct AssetStats {
        std::size_t textureCount = 0;
        std::size_t soundCount = 0;
        std::size_t musicCount = 0;
        std::size_t fontCount = 0;
        std::size_t prefabCount = 0;
        std::size_t scriptCount = 0;
        std::size_t totalCount = 0;

        std::size_t textureBytes = 0;
        std::size_t soundBytes = 0;
        std::size_t musicBytes = 0;
        std::size_t fontBytes = 0;
        std::size_t prefabBytes = 0;
        std::size_t scriptBytes = 0;
        std::size_t totalBytes = 0;

        std::uint64_t totalLoadCalls = 0;
        std::uint64_t totalCacheHits = 0;
    };

    class AssetManager {
    public:
        // --- Lifecycle ---
        static void Init();
        static void Shutdown();
        static bool IsInitialized();

        // Ensures built-in starter assets (textures, sample WAV, sample Lua script) exist on disk
        static void EnsureStarterAssetsExist();

        // --- Path Normalization & Type Classification ---
        static std::string NormalizePath(const std::string& path);
        static AssetType DetectType(const std::string& path);
        static const char* GetTypeName(AssetType type);

        // --- Unified Generic Resource API ---
        static bool Load(const std::string& path, AssetType explicitType = AssetType::Unknown);
        static bool Has(const std::string& path, AssetType explicitType = AssetType::Unknown);
        static bool Unload(const std::string& path, AssetType explicitType = AssetType::Unknown);
        static void UnloadByType(AssetType type);
        static void UnloadAll();

        // --- Textures / Images (.png, .bmp, .jpg, .jpeg) ---
        static bool LoadTexture(const std::string& path);
        static bool HasTexture(const std::string& path);
        static bool GetTextureSize(const std::string& path, int& outWidth, int& outHeight);
        static void UnloadTexture(const std::string& path);
        static void UnloadAllTextures();
        static std::size_t GetCachedTextureCount();
        static std::size_t GetTotalTextureVRAMBytes();

        // --- Sound Effects (.wav, .ogg in-memory) ---
        static bool LoadSound(const std::string& path);
        static bool HasSound(const std::string& path);
        static Audio::SoundInfo GetSoundInfo(const std::string& path);
        static void UnloadSound(const std::string& path);
        static void UnloadAllSounds();
        static std::size_t GetCachedSoundCount();
        static std::size_t GetTotalSoundMemoryBytes();

        // --- Music Streams (.ogg, .mp3, .wav streaming) ---
        static bool LoadMusic(const std::string& path);
        static bool HasMusic(const std::string& path);
        static Audio::MusicInfo GetMusicInfo(const std::string& path);
        static void UnloadMusic(const std::string& path);
        static void UnloadAllMusic();
        static std::size_t GetCachedMusicCount();
        static std::size_t GetTotalMusicMemoryBytes();

        // --- Typography / Fonts (.ttf, .otf) ---
        static bool LoadFont(const std::string& path, int defaultFontSize = 20);
        static bool HasFont(const std::string& path);
        static void UnloadFont(const std::string& path);
        static void UnloadAllFonts();
        static std::size_t GetCachedFontCount();
        static std::size_t GetTotalFontVRAMBytes();

        // --- Prefabs (.prefab Entity Templates) ---
        static bool LoadPrefab(const std::string& path);
        static bool HasPrefab(const std::string& path);
        static const Entity* GetPrefab(const std::string& path);
        static void CachePrefab(const std::string& path, const Entity& entityTemplate);
        static bool ReloadPrefab(const std::string& path);
        static Entity* InstantiatePrefab(Scene& scene,
                                         const std::string& path,
                                         float overrideX = 0.0f,
                                         float overrideY = 0.0f,
                                         bool useOverridePosition = false);
        static Entity* InstantiatePrefabAt(Scene& scene, const std::string& path, float x, float y);
        static void UnloadPrefab(const std::string& path);
        static void UnloadAllPrefabs();
        static std::size_t GetCachedPrefabCount();
        static std::size_t GetTotalPrefabMemoryBytes();

        // --- Scripts (.lua Source Buffers) ---
        static bool LoadScript(const std::string& path);
        static bool HasScript(const std::string& path);
        static const ScriptAsset* GetScript(const std::string& path);
        static std::string GetScriptSource(const std::string& path);
        static bool ReloadScript(const std::string& path);
        static void UnloadScript(const std::string& path);
        static void UnloadAllScripts();
        static std::size_t GetCachedScriptCount();
        static std::size_t GetTotalScriptMemoryBytes();

        // --- Scene Integration Helpers ---
        // Preloads all textures and scripts referenced by entities in the given scene; returns loaded count
        static std::size_t PreloadSceneAssets(const Scene& scene);
        // Unloads cached textures and scripts not referenced by any entity in the given scene; returns unloaded count
        static std::size_t UnloadUnusedSceneAssets(const Scene& scene);

        // --- Registry Inspection & Telemetry ---
        static AssetRecord GetRecord(const std::string& path, AssetType explicitType = AssetType::Unknown);
        static std::vector<AssetRecord> GetAllRecords();
        static std::vector<AssetRecord> GetRecordsByType(AssetType type);
        static AssetStats GetStats();
    };

} // namespace Talos::Scene

namespace Talos::Core {
    using AssetType    = Talos::Scene::AssetType;
    using ScriptAsset  = Talos::Scene::ScriptAsset;
    using AssetRecord  = Talos::Scene::AssetRecord;
    using AssetStats   = Talos::Scene::AssetStats;
    using AssetManager = Talos::Scene::AssetManager;
} // namespace Talos::Core

namespace Talos {
    using AssetType    = Talos::Scene::AssetType;
    using ScriptAsset  = Talos::Scene::ScriptAsset;
    using AssetRecord  = Talos::Scene::AssetRecord;
    using AssetStats   = Talos::Scene::AssetStats;
    using AssetManager = Talos::Scene::AssetManager;
} // namespace Talos
