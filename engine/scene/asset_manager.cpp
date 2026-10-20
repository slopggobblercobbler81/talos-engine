// Central resource manager implementation for textures, audio, fonts, prefabs, and scripts
#include "scene/asset_manager.h"
#include "core/logger.h"
#include "renderer/renderer2d.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#ifdef PlaySound
#undef PlaySound
#endif

namespace Talos::Scene {

    namespace {

        struct UsageCounter {
            std::uint32_t loadCount = 0;
            std::uint32_t reuseCount = 0;
        };

        static bool s_initialized = false;
        static std::uint64_t s_totalLoadCalls = 0;
        static std::uint64_t s_totalCacheHits = 0;

        // Cached .prefab Entity templates keyed by normalized path
        static std::unordered_map<std::string, Entity> s_prefabCache;

        // Cached .lua script source buffers keyed by normalized path
        static std::unordered_map<std::string, ScriptAsset> s_scriptCache;

        // Per-type, per-path load and reuse counters
        static std::unordered_map<int, std::unordered_map<std::string, UsageCounter>> s_usageByType;

        static std::string ToLowerAscii(const std::string& input) {
            std::string out = input;
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return out;
        }

        static std::string ExtractExtensionLower(const std::string& path) {
            std::size_t dotPos = path.find_last_of('.');
            std::size_t slashPos = path.find_last_of("/\\");
            if (dotPos == std::string::npos || (slashPos != std::string::npos && dotPos < slashPos)) {
                return "";
            }
            return ToLowerAscii(path.substr(dotPos));
        }

        static void RecordLoadAccess(AssetType type, const std::string& normalizedKey, bool wasAlreadyLoaded) {
            if (type == AssetType::Unknown || normalizedKey.empty()) {
                return;
            }
            UsageCounter& counter = s_usageByType[static_cast<int>(type)][normalizedKey];
            counter.loadCount += 1;
            s_totalLoadCalls += 1;
            if (wasAlreadyLoaded) {
                counter.reuseCount += 1;
                s_totalCacheHits += 1;
            }
        }

        static void ClearUsageCounter(AssetType type, const std::string& normalizedKey) {
            auto typeIt = s_usageByType.find(static_cast<int>(type));
            if (typeIt != s_usageByType.end()) {
                typeIt->second.erase(normalizedKey);
            }
        }

        static UsageCounter GetUsageCounter(AssetType type, const std::string& normalizedKey) {
            auto typeIt = s_usageByType.find(static_cast<int>(type));
            if (typeIt == s_usageByType.end()) {
                return {};
            }
            auto keyIt = typeIt->second.find(normalizedKey);
            if (keyIt == typeIt->second.end()) {
                return {};
            }
            return keyIt->second;
        }

        static std::size_t EstimateEntityMemoryBytes(const Entity& e) {
            return sizeof(Entity) +
                   e.name.capacity() +
                   e.tag.capacity() +
                   e.sprite.texturePath.capacity() +
                   e.text.content.capacity() +
                   e.camera.targetName.capacity() +
                   e.audio.audioPath.capacity();
        }

        static std::size_t EstimateScriptMemoryBytes(const ScriptAsset& s) {
            return sizeof(ScriptAsset) + s.path.capacity() + s.source.capacity();
        }

        static std::size_t CountLines(const std::string& text) {
            if (text.empty()) {
                return 0;
            }
            std::size_t lines = 1;
            for (char c : text) {
                if (c == '\n') {
                    ++lines;
                }
            }
            return lines;
        }

        static void WriteStarterWavFileIfMissing(const std::string& filePath) {
            std::error_code ec;
            if (std::filesystem::exists(filePath, ec)) {
                return;
            }
            std::filesystem::path p(filePath);
            if (p.has_parent_path()) {
                std::filesystem::create_directories(p.parent_path(), ec);
            }

            const std::uint32_t sampleRate = 22050;
            const std::uint16_t numChannels = 1;
            const std::uint16_t bitsPerSample = 16;
            const std::uint32_t numSamples = sampleRate / 10; // 0.10s chime
            const std::uint32_t dataBytes = numSamples * numChannels * (bitsPerSample / 8);
            const std::uint32_t riffChunkSize = 36 + dataBytes;
            const std::uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
            const std::uint16_t blockAlign = numChannels * (bitsPerSample / 8);

            std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                return;
            }

            auto writeU16 = [&out](std::uint16_t v) {
                unsigned char b[2] = {
                    static_cast<unsigned char>(v & 0xFFu),
                    static_cast<unsigned char>((v >> 8) & 0xFFu)
                };
                out.write(reinterpret_cast<const char*>(b), 2);
            };
            auto writeU32 = [&out](std::uint32_t v) {
                unsigned char b[4] = {
                    static_cast<unsigned char>(v & 0xFFu),
                    static_cast<unsigned char>((v >> 8) & 0xFFu),
                    static_cast<unsigned char>((v >> 16) & 0xFFu),
                    static_cast<unsigned char>((v >> 24) & 0xFFu)
                };
                out.write(reinterpret_cast<const char*>(b), 4);
            };

            out.write("RIFF", 4);
            writeU32(riffChunkSize);
            out.write("WAVE", 4);
            out.write("fmt ", 4);
            writeU32(16);            // PCM chunk size
            writeU16(1);             // AudioFormat = 1 (PCM)
            writeU16(numChannels);
            writeU32(sampleRate);
            writeU32(byteRate);
            writeU16(blockAlign);
            writeU16(bitsPerSample);
            out.write("data", 4);
            writeU32(dataBytes);

            for (std::uint32_t i = 0; i < numSamples; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(sampleRate);
                float env = 1.0f - (static_cast<float>(i) / static_cast<float>(numSamples));
                float freq = (i < numSamples / 2) ? 880.0f : 1320.0f;
                float sample = std::sin(2.0f * 3.14159265f * freq * t) * env * 0.35f;
                std::int16_t pcm = static_cast<std::int16_t>(sample * 32767.0f);
                writeU16(static_cast<std::uint16_t>(pcm));
            }
        }

        static void WriteStarterLuaScriptIfMissing(const std::string& filePath) {
            std::error_code ec;
            if (std::filesystem::exists(filePath, ec)) {
                return;
            }
            std::filesystem::path p(filePath);
            if (p.has_parent_path()) {
                std::filesystem::create_directories(p.parent_path(), ec);
            }

            std::ofstream out(filePath, std::ios::trunc);
            if (!out.is_open()) {
                return;
            }
            out << "-- Talos Engine starter Lua gameplay script\n"
                << "local PlayerController = {}\n\n"
                << "function PlayerController.on_ready(entity)\n"
                << "    -- Called once when the entity enters Play Mode\n"
                << "end\n\n"
                << "function PlayerController.on_update(entity, dt)\n"
                << "    -- Called every frame during Play Mode\n"
                << "end\n\n"
                << "return PlayerController\n";
        }

    } // anonymous namespace

    void AssetManager::Init() {
        if (s_initialized) {
            return;
        }
        EnsureStarterAssetsExist();
        s_initialized = true;
        TALOS_LOG_INFO("AssetManager initialized (central registry ready for Textures, Sounds, Music, Fonts, Prefabs, Scripts)");
    }

    void AssetManager::Shutdown() {
        UnloadAll();
        s_usageByType.clear();
        s_totalLoadCalls = 0;
        s_totalCacheHits = 0;
        s_initialized = false;
        TALOS_LOG_INFO("AssetManager shut down and all cached resources released");
    }

    bool AssetManager::IsInitialized() {
        return s_initialized;
    }

    void AssetManager::EnsureStarterAssetsExist() {
        Renderer::Renderer2D::EnsureStarterAssetsExist();
        WriteStarterWavFileIfMissing("examples/assets/audio/coin_pickup.wav");
        WriteStarterLuaScriptIfMissing("examples/assets/scripts/player_controller.lua");
    }

    std::string AssetManager::NormalizePath(const std::string& path) {
        if (path.empty()) {
            return "";
        }

        std::size_t start = 0;
        while (start < path.size() &&
               (std::isspace(static_cast<unsigned char>(path[start])) || path[start] == '"' || path[start] == '\'')) {
            ++start;
        }
        std::size_t end = path.size();
        while (end > start &&
               (std::isspace(static_cast<unsigned char>(path[end - 1])) || path[end - 1] == '"' || path[end - 1] == '\'')) {
            --end;
        }
        if (start >= end) {
            return "";
        }

        std::string cleaned;
        cleaned.reserve(end - start);
        for (std::size_t i = start; i < end; ++i) {
            char c = (path[i] == '\\') ? '/' : path[i];
            if (c == '/' && !cleaned.empty() && cleaned.back() == '/') {
                continue;
            }
            cleaned.push_back(c);
        }

        while (cleaned.size() >= 2 && cleaned[0] == '.' && cleaned[1] == '/') {
            cleaned.erase(0, 2);
        }

        return cleaned;
    }

    AssetType AssetManager::DetectType(const std::string& path) {
        std::string norm = NormalizePath(path);
        if (norm.empty()) {
            return AssetType::Unknown;
        }

        std::string ext = ExtractExtensionLower(norm);
        if (ext == ".png" || ext == ".bmp" || ext == ".jpg" || ext == ".jpeg") {
            return AssetType::Texture;
        }
        if (ext == ".mp3") {
            return AssetType::Music;
        }
        if (ext == ".wav") {
            std::string lowerPath = ToLowerAscii(norm);
            if (lowerPath.find("/music/") != std::string::npos ||
                lowerPath.find("bgm") != std::string::npos ||
                lowerPath.find("stream") != std::string::npos) {
                return AssetType::Music;
            }
            return AssetType::Sound;
        }
        if (ext == ".ogg") {
            std::string lowerPath = ToLowerAscii(norm);
            if (lowerPath.find("/sfx/") != std::string::npos ||
                lowerPath.find("/sounds/") != std::string::npos ||
                lowerPath.find("sfx_") != std::string::npos) {
                return AssetType::Sound;
            }
            return AssetType::Music;
        }
        if (ext == ".ttf" || ext == ".otf") {
            return AssetType::Font;
        }
        if (ext == ".prefab") {
            return AssetType::Prefab;
        }
        if (ext == ".lua") {
            return AssetType::Script;
        }
        return AssetType::Unknown;
    }

    const char* AssetManager::GetTypeName(AssetType type) {
        switch (type) {
            case AssetType::Texture: return "Texture";
            case AssetType::Sound:   return "Sound";
            case AssetType::Music:   return "Music";
            case AssetType::Font:    return "Font";
            case AssetType::Prefab:  return "Prefab";
            case AssetType::Script:  return "Script";
            default:                 return "Unknown";
        }
    }

    bool AssetManager::Load(const std::string& path, AssetType explicitType) {
        AssetType targetType = (explicitType != AssetType::Unknown) ? explicitType : DetectType(path);
        switch (targetType) {
            case AssetType::Texture: return LoadTexture(path);
            case AssetType::Sound:   return LoadSound(path);
            case AssetType::Music:   return LoadMusic(path);
            case AssetType::Font:    return LoadFont(path);
            case AssetType::Prefab:  return LoadPrefab(path);
            case AssetType::Script:  return LoadScript(path);
            default:
                TALOS_LOG_WARN("AssetManager::Load could not determine asset type for '" + path + "'");
                return false;
        }
    }

    bool AssetManager::Has(const std::string& path, AssetType explicitType) {
        if (explicitType != AssetType::Unknown) {
            switch (explicitType) {
                case AssetType::Texture: return HasTexture(path);
                case AssetType::Sound:   return HasSound(path);
                case AssetType::Music:   return HasMusic(path);
                case AssetType::Font:    return HasFont(path);
                case AssetType::Prefab:  return HasPrefab(path);
                case AssetType::Script:  return HasScript(path);
                default:                 return false;
            }
        }

        AssetType detected = DetectType(path);
        switch (detected) {
            case AssetType::Texture: return HasTexture(path);
            case AssetType::Sound:   return HasSound(path) || HasMusic(path);
            case AssetType::Music:   return HasMusic(path) || HasSound(path);
            case AssetType::Font:    return HasFont(path);
            case AssetType::Prefab:  return HasPrefab(path);
            case AssetType::Script:  return HasScript(path);
            default:                 return false;
        }
    }

    bool AssetManager::Unload(const std::string& path, AssetType explicitType) {
        if (explicitType != AssetType::Unknown) {
            if (!Has(path, explicitType)) {
                return false;
            }
            switch (explicitType) {
                case AssetType::Texture: UnloadTexture(path); return true;
                case AssetType::Sound:   UnloadSound(path);   return true;
                case AssetType::Music:   UnloadMusic(path);   return true;
                case AssetType::Font:    UnloadFont(path);    return true;
                case AssetType::Prefab:  UnloadPrefab(path);  return true;
                case AssetType::Script:  UnloadScript(path);  return true;
                default:                 return false;
            }
        }

        bool unloadedAny = false;
        if (HasTexture(path)) { UnloadTexture(path); unloadedAny = true; }
        if (HasSound(path))   { UnloadSound(path);   unloadedAny = true; }
        if (HasMusic(path))   { UnloadMusic(path);   unloadedAny = true; }
        if (HasFont(path))    { UnloadFont(path);    unloadedAny = true; }
        if (HasPrefab(path))  { UnloadPrefab(path);  unloadedAny = true; }
        if (HasScript(path))  { UnloadScript(path);  unloadedAny = true; }
        return unloadedAny;
    }

    void AssetManager::UnloadByType(AssetType type) {
        switch (type) {
            case AssetType::Texture: UnloadAllTextures(); break;
            case AssetType::Sound:   UnloadAllSounds();   break;
            case AssetType::Music:   UnloadAllMusic();    break;
            case AssetType::Font:    UnloadAllFonts();    break;
            case AssetType::Prefab:  UnloadAllPrefabs();  break;
            case AssetType::Script:  UnloadAllScripts();  break;
            default: break;
        }
    }

    void AssetManager::UnloadAll() {
        UnloadAllTextures();
        UnloadAllSounds();
        UnloadAllMusic();
        UnloadAllFonts();
        UnloadAllPrefabs();
        UnloadAllScripts();
    }

    // =========================================================================
    // 1. Textures / Images (.png, .bmp, .jpg, .jpeg)
    // =========================================================================

    bool AssetManager::LoadTexture(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }
        bool wasLoaded = Renderer::Renderer2D::IsTextureLoaded(key.c_str());
        if (!Renderer::Renderer2D::LoadTexture(key.c_str())) {
            return false;
        }
        RecordLoadAccess(AssetType::Texture, key, wasLoaded);
        return true;
    }

    bool AssetManager::HasTexture(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }
        return Renderer::Renderer2D::IsTextureLoaded(key.c_str());
    }

    bool AssetManager::GetTextureSize(const std::string& path, int& outWidth, int& outHeight) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            outWidth = 0;
            outHeight = 0;
            return false;
        }
        bool wasLoaded = Renderer::Renderer2D::IsTextureLoaded(key.c_str());
        if (Renderer::Renderer2D::GetTextureSize(key.c_str(), outWidth, outHeight)) {
            RecordLoadAccess(AssetType::Texture, key, wasLoaded);
            return true;
        }
        return false;
    }

    void AssetManager::UnloadTexture(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return;
        }
        Renderer::Renderer2D::UnloadTexture(key.c_str());
        ClearUsageCounter(AssetType::Texture, key);
    }

    void AssetManager::UnloadAllTextures() {
        Renderer::Renderer2D::UnloadAllTextures();
        s_usageByType.erase(static_cast<int>(AssetType::Texture));
    }

    std::size_t AssetManager::GetCachedTextureCount() {
        return Renderer::Renderer2D::GetCachedTextureCount();
    }

    std::size_t AssetManager::GetTotalTextureVRAMBytes() {
        return Renderer::Renderer2D::GetTotalTextureVRAMBytes();
    }

    // =========================================================================
    // 2. Sound Effects (.wav, .ogg in-memory)
    // =========================================================================

    bool AssetManager::LoadSound(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }
        bool wasLoaded = Audio::AudioSystem::HasSound(key);
        if (!Audio::AudioSystem::LoadSound(key)) {
            return false;
        }
        RecordLoadAccess(AssetType::Sound, key, wasLoaded);
        return true;
    }

    bool AssetManager::HasSound(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }
        return Audio::AudioSystem::HasSound(key) || Audio::AudioSystem::HasSound(path);
    }

    Audio::SoundInfo AssetManager::GetSoundInfo(const std::string& path) {
        std::string key = NormalizePath(path);
        if (Audio::AudioSystem::HasSound(key)) {
            return Audio::AudioSystem::GetSoundInfo(key);
        }
        return Audio::AudioSystem::GetSoundInfo(path);
    }

    void AssetManager::UnloadSound(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return;
        }
        Audio::AudioSystem::UnloadSound(key);
        if (key != path) {
            Audio::AudioSystem::UnloadSound(path);
        }
        ClearUsageCounter(AssetType::Sound, key);
    }

    void AssetManager::UnloadAllSounds() {
        Audio::AudioSystem::UnloadAllSounds();
        s_usageByType.erase(static_cast<int>(AssetType::Sound));
    }

    std::size_t AssetManager::GetCachedSoundCount() {
        return Audio::AudioSystem::GetCachedSoundCount();
    }

    std::size_t AssetManager::GetTotalSoundMemoryBytes() {
        return Audio::AudioSystem::GetTotalSoundMemoryBytes();
    }

    // =========================================================================
    // 3. Music Tracks (.ogg, .mp3, .wav streaming)
    // =========================================================================

    bool AssetManager::LoadMusic(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }
        bool wasLoaded = Audio::AudioSystem::HasMusic(key);
        if (!Audio::AudioSystem::LoadMusic(key)) {
            return false;
        }
        RecordLoadAccess(AssetType::Music, key, wasLoaded);
        return true;
    }

    bool AssetManager::HasMusic(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }
        return Audio::AudioSystem::HasMusic(key) || Audio::AudioSystem::HasMusic(path);
    }

    Audio::MusicInfo AssetManager::GetMusicInfo(const std::string& path) {
        std::string key = NormalizePath(path);
        if (Audio::AudioSystem::HasMusic(key)) {
            return Audio::AudioSystem::GetMusicInfo(key);
        }
        return Audio::AudioSystem::GetMusicInfo(path);
    }

    void AssetManager::UnloadMusic(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return;
        }
        Audio::AudioSystem::UnloadMusic(key);
        if (key != path) {
            Audio::AudioSystem::UnloadMusic(path);
        }
        ClearUsageCounter(AssetType::Music, key);
    }

    void AssetManager::UnloadAllMusic() {
        Audio::AudioSystem::UnloadAllMusic();
        s_usageByType.erase(static_cast<int>(AssetType::Music));
    }

    std::size_t AssetManager::GetCachedMusicCount() {
        return Audio::AudioSystem::GetCachedMusicCount();
    }

    std::size_t AssetManager::GetTotalMusicMemoryBytes() {
        return Audio::AudioSystem::GetTotalMusicMemoryBytes();
    }

    // =========================================================================
    // 4. Typography / Fonts (.ttf, .otf)
    // =========================================================================

    bool AssetManager::LoadFont(const std::string& path, int defaultFontSize) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }
        bool wasLoaded = Renderer::Renderer2D::HasFont(key.c_str());
        if (!Renderer::Renderer2D::LoadFont(key.c_str(), defaultFontSize)) {
            return false;
        }
        RecordLoadAccess(AssetType::Font, key, wasLoaded);
        return true;
    }

    bool AssetManager::HasFont(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }
        return Renderer::Renderer2D::HasFont(key.c_str());
    }

    void AssetManager::UnloadFont(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return;
        }
        Renderer::Renderer2D::UnloadFont(key.c_str());
        ClearUsageCounter(AssetType::Font, key);
    }

    void AssetManager::UnloadAllFonts() {
        Renderer::Renderer2D::UnloadAllFonts();
        s_usageByType.erase(static_cast<int>(AssetType::Font));
    }

    std::size_t AssetManager::GetCachedFontCount() {
        return Renderer::Renderer2D::GetCachedFontCount();
    }

    std::size_t AssetManager::GetTotalFontVRAMBytes() {
        std::size_t total = 0;
        for (const std::string& fontPath : Renderer::Renderer2D::GetLoadedFontPaths()) {
            total += Renderer::Renderer2D::GetCustomFontVRAMBytes(fontPath.c_str());
        }
        return total;
    }

    // =========================================================================
    // 5. Prefabs (.prefab Entity Templates)
    // =========================================================================

    bool AssetManager::LoadPrefab(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }

        auto existing = s_prefabCache.find(key);
        if (existing != s_prefabCache.end()) {
            RecordLoadAccess(AssetType::Prefab, key, true);
            return true;
        }

        if (ExtractExtensionLower(key) != ".prefab") {
            TALOS_LOG_WARN("AssetManager::LoadPrefab expected a '.prefab' file: '" + key + "'");
            return false;
        }

        Entity loadedTemplate;
        if (!SceneSerializer::LoadPrefab(key, loadedTemplate)) {
            return false;
        }

        // LoadPrefab in SceneSerializer may already call CachePrefab; ensure entry exists
        s_prefabCache[key] = loadedTemplate;
        UsageCounter& counter = s_usageByType[static_cast<int>(AssetType::Prefab)][key];
        if (counter.loadCount == 0) {
            RecordLoadAccess(AssetType::Prefab, key, false);
        }
        return true;
    }

    bool AssetManager::HasPrefab(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }
        return s_prefabCache.find(key) != s_prefabCache.end();
    }

    const Entity* AssetManager::GetPrefab(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return nullptr;
        }
        auto it = s_prefabCache.find(key);
        if (it != s_prefabCache.end()) {
            RecordLoadAccess(AssetType::Prefab, key, true);
            return &it->second;
        }
        if (!LoadPrefab(key)) {
            return nullptr;
        }
        it = s_prefabCache.find(key);
        return (it != s_prefabCache.end()) ? &it->second : nullptr;
    }

    void AssetManager::CachePrefab(const std::string& path, const Entity& entityTemplate) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return;
        }
        bool existed = (s_prefabCache.find(key) != s_prefabCache.end());
        Entity cleanCopy = entityTemplate;
        cleanCopy.id = 0;
        cleanCopy.sprite.frameTimer = 0.0f;
        cleanCopy.camera.runtimeInitialized = false;
        cleanCopy.audio.runtimeStarted = false;
        s_prefabCache[key] = std::move(cleanCopy);

        UsageCounter& counter = s_usageByType[static_cast<int>(AssetType::Prefab)][key];
        if (!existed && counter.loadCount == 0) {
            counter.loadCount = 1;
            s_totalLoadCalls += 1;
        }
    }

    bool AssetManager::ReloadPrefab(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }
        s_prefabCache.erase(key);
        ClearUsageCounter(AssetType::Prefab, key);
        return LoadPrefab(key);
    }

    Entity* AssetManager::InstantiatePrefab(Scene& scene,
                                            const std::string& path,
                                            float overrideX,
                                            float overrideY,
                                            bool useOverridePosition) {
        return SceneSerializer::InstantiatePrefab(scene, path, overrideX, overrideY, useOverridePosition);
    }

    Entity* AssetManager::InstantiatePrefabAt(Scene& scene, const std::string& path, float x, float y) {
        return SceneSerializer::InstantiatePrefab(scene, path, x, y, true);
    }

    void AssetManager::UnloadPrefab(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return;
        }
        s_prefabCache.erase(key);
        ClearUsageCounter(AssetType::Prefab, key);
    }

    void AssetManager::UnloadAllPrefabs() {
        s_prefabCache.clear();
        s_usageByType.erase(static_cast<int>(AssetType::Prefab));
    }

    std::size_t AssetManager::GetCachedPrefabCount() {
        return s_prefabCache.size();
    }

    std::size_t AssetManager::GetTotalPrefabMemoryBytes() {
        std::size_t total = 0;
        for (const auto& [path, entity] : s_prefabCache) {
            total += path.capacity() + EstimateEntityMemoryBytes(entity);
        }
        return total;
    }

    // =========================================================================
    // 6. Scripts (.lua Source Buffers)
    // =========================================================================

    bool AssetManager::LoadScript(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }

        auto existing = s_scriptCache.find(key);
        if (existing != s_scriptCache.end()) {
            RecordLoadAccess(AssetType::Script, key, true);
            return true;
        }

        if (ExtractExtensionLower(key) != ".lua") {
            TALOS_LOG_WARN("AssetManager::LoadScript expected a '.lua' file: '" + key + "'");
            return false;
        }

        std::error_code ec;
        if (!std::filesystem::exists(key, ec) || std::filesystem::is_directory(key, ec)) {
            TALOS_LOG_ERROR("Script file not found on disk: '" + key + "'");
            return false;
        }

        std::ifstream in(key, std::ios::in | std::ios::binary);
        if (!in.is_open()) {
            TALOS_LOG_ERROR("Failed to open Lua script file: '" + key + "'");
            return false;
        }

        std::ostringstream buffer;
        buffer << in.rdbuf();

        ScriptAsset asset;
        asset.path = key;
        asset.source = buffer.str();
        asset.byteSize = asset.source.size();
        asset.lineCount = CountLines(asset.source);

        s_scriptCache[key] = std::move(asset);
        RecordLoadAccess(AssetType::Script, key, false);
        TALOS_LOG_INFO("Loaded Lua script into cache: '" + key + "' (" +
                       std::to_string(s_scriptCache[key].byteSize) + " bytes)");
        return true;
    }

    bool AssetManager::HasScript(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }
        return s_scriptCache.find(key) != s_scriptCache.end();
    }

    const ScriptAsset* AssetManager::GetScript(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return nullptr;
        }
        auto it = s_scriptCache.find(key);
        if (it != s_scriptCache.end()) {
            RecordLoadAccess(AssetType::Script, key, true);
            return &it->second;
        }
        if (!LoadScript(key)) {
            return nullptr;
        }
        it = s_scriptCache.find(key);
        return (it != s_scriptCache.end()) ? &it->second : nullptr;
    }

    std::string AssetManager::GetScriptSource(const std::string& path) {
        const ScriptAsset* script = GetScript(path);
        return script ? script->source : "";
    }

    bool AssetManager::ReloadScript(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return false;
        }
        s_scriptCache.erase(key);
        ClearUsageCounter(AssetType::Script, key);
        return LoadScript(key);
    }

    void AssetManager::UnloadScript(const std::string& path) {
        std::string key = NormalizePath(path);
        if (key.empty()) {
            return;
        }
        s_scriptCache.erase(key);
        ClearUsageCounter(AssetType::Script, key);
    }

    void AssetManager::UnloadAllScripts() {
        s_scriptCache.clear();
        s_usageByType.erase(static_cast<int>(AssetType::Script));
    }

    std::size_t AssetManager::GetCachedScriptCount() {
        return s_scriptCache.size();
    }

    std::size_t AssetManager::GetTotalScriptMemoryBytes() {
        std::size_t total = 0;
        for (const auto& [path, script] : s_scriptCache) {
            total += path.capacity() + EstimateScriptMemoryBytes(script);
        }
        return total;
    }

    // =========================================================================
    // Scene Integration Helpers
    // =========================================================================

    std::size_t AssetManager::PreloadSceneAssets(const Scene& scene) {
        std::size_t loadedCount = 0;
        for (const Entity& e : scene.GetEntities()) {
            if (e.sprite.enabled && !e.sprite.texturePath.empty()) {
                if (LoadTexture(e.sprite.texturePath)) {
                    ++loadedCount;
                }
            }
            if (e.audio.enabled && !e.audio.audioPath.empty()) {
                bool ok = e.audio.isMusic ? LoadMusic(e.audio.audioPath)
                                          : LoadSound(e.audio.audioPath);
                if (ok) {
                    ++loadedCount;
                }
            }
        }
        return loadedCount;
    }

    std::size_t AssetManager::UnloadUnusedSceneAssets(const Scene& scene) {
        std::unordered_set<std::string> usedTextures;
        std::unordered_set<std::string> usedSounds;
        std::unordered_set<std::string> usedMusic;

        for (const Entity& e : scene.GetEntities()) {
            if (e.sprite.enabled && !e.sprite.texturePath.empty()) {
                usedTextures.insert(NormalizePath(e.sprite.texturePath));
            }
            if (e.audio.enabled && !e.audio.audioPath.empty()) {
                if (e.audio.isMusic) {
                    usedMusic.insert(NormalizePath(e.audio.audioPath));
                } else {
                    usedSounds.insert(NormalizePath(e.audio.audioPath));
                }
            }
        }

        std::size_t unloaded = 0;

        for (const std::string& texPath : Renderer::Renderer2D::GetLoadedTexturePaths()) {
            if (usedTextures.find(NormalizePath(texPath)) == usedTextures.end()) {
                UnloadTexture(texPath);
                ++unloaded;
            }
        }

        for (const std::string& sndPath : Audio::AudioSystem::GetLoadedSoundPaths()) {
            if (usedSounds.find(NormalizePath(sndPath)) == usedSounds.end()) {
                UnloadSound(sndPath);
                ++unloaded;
            }
        }

        for (const std::string& musPath : Audio::AudioSystem::GetLoadedMusicPaths()) {
            if (usedMusic.find(NormalizePath(musPath)) == usedMusic.end()) {
                UnloadMusic(musPath);
                ++unloaded;
            }
        }

        return unloaded;
    }

    // =========================================================================
    // Registry Inspection & Telemetry
    // =========================================================================

    AssetRecord AssetManager::GetRecord(const std::string& path, AssetType explicitType) {
        std::string key = NormalizePath(path);
        AssetType resolvedType = (explicitType != AssetType::Unknown) ? explicitType : DetectType(key);

        // If an audio file was explicitly loaded as Sound vs Music, match whichever cache holds it
        if (explicitType == AssetType::Unknown && (resolvedType == AssetType::Sound || resolvedType == AssetType::Music)) {
            if (HasSound(key) && !HasMusic(key)) {
                resolvedType = AssetType::Sound;
            } else if (HasMusic(key) && !HasSound(key)) {
                resolvedType = AssetType::Music;
            }
        }

        AssetRecord rec;
        rec.path = key;
        rec.type = resolvedType;
        rec.loaded = Has(key, resolvedType);

        UsageCounter counter = GetUsageCounter(resolvedType, key);
        rec.loadCount = (rec.loaded && counter.loadCount == 0) ? 1u : counter.loadCount;
        rec.reuseCount = counter.reuseCount;

        if (rec.loaded) {
            switch (resolvedType) {
                case AssetType::Texture:
                    rec.memoryBytes = Renderer::Renderer2D::GetTextureVRAMBytes(key.c_str());
                    break;
                case AssetType::Sound:
                    rec.memoryBytes = GetSoundInfo(key).memoryBytes;
                    break;
                case AssetType::Music:
                    rec.memoryBytes = GetMusicInfo(key).streamBufferBytes;
                    break;
                case AssetType::Font:
                    rec.memoryBytes = Renderer::Renderer2D::GetCustomFontVRAMBytes(key.c_str());
                    break;
                case AssetType::Prefab: {
                    auto it = s_prefabCache.find(key);
                    if (it != s_prefabCache.end()) {
                        rec.memoryBytes = it->first.capacity() + EstimateEntityMemoryBytes(it->second);
                    }
                    break;
                }
                case AssetType::Script: {
                    auto it = s_scriptCache.find(key);
                    if (it != s_scriptCache.end()) {
                        rec.memoryBytes = it->first.capacity() + EstimateScriptMemoryBytes(it->second);
                    }
                    break;
                }
                default:
                    break;
            }
        }
        return rec;
    }

    std::vector<AssetRecord> AssetManager::GetRecordsByType(AssetType type) {
        std::vector<AssetRecord> records;

        switch (type) {
            case AssetType::Texture:
                for (const std::string& p : Renderer::Renderer2D::GetLoadedTexturePaths()) {
                    records.push_back(GetRecord(p, AssetType::Texture));
                }
                break;
            case AssetType::Sound:
                for (const std::string& p : Audio::AudioSystem::GetLoadedSoundPaths()) {
                    records.push_back(GetRecord(p, AssetType::Sound));
                }
                break;
            case AssetType::Music:
                for (const std::string& p : Audio::AudioSystem::GetLoadedMusicPaths()) {
                    records.push_back(GetRecord(p, AssetType::Music));
                }
                break;
            case AssetType::Font:
                for (const std::string& p : Renderer::Renderer2D::GetLoadedFontPaths()) {
                    records.push_back(GetRecord(p, AssetType::Font));
                }
                break;
            case AssetType::Prefab:
                for (const auto& [p, entity] : s_prefabCache) {
                    (void)entity;
                    records.push_back(GetRecord(p, AssetType::Prefab));
                }
                std::sort(records.begin(), records.end(), [](const AssetRecord& a, const AssetRecord& b) {
                    return a.path < b.path;
                });
                break;
            case AssetType::Script:
                for (const auto& [p, script] : s_scriptCache) {
                    (void)script;
                    records.push_back(GetRecord(p, AssetType::Script));
                }
                std::sort(records.begin(), records.end(), [](const AssetRecord& a, const AssetRecord& b) {
                    return a.path < b.path;
                });
                break;
            default:
                break;
        }

        return records;
    }

    std::vector<AssetRecord> AssetManager::GetAllRecords() {
        std::vector<AssetRecord> all;
        const AssetType types[] = {
            AssetType::Texture,
            AssetType::Sound,
            AssetType::Music,
            AssetType::Font,
            AssetType::Prefab,
            AssetType::Script
        };
        for (AssetType t : types) {
            std::vector<AssetRecord> group = GetRecordsByType(t);
            all.insert(all.end(), group.begin(), group.end());
        }
        return all;
    }

    AssetStats AssetManager::GetStats() {
        AssetStats stats;
        stats.textureCount = GetCachedTextureCount();
        stats.soundCount   = GetCachedSoundCount();
        stats.musicCount   = GetCachedMusicCount();
        stats.fontCount    = GetCachedFontCount();
        stats.prefabCount  = GetCachedPrefabCount();
        stats.scriptCount  = GetCachedScriptCount();
        stats.totalCount   = stats.textureCount + stats.soundCount + stats.musicCount +
                             stats.fontCount + stats.prefabCount + stats.scriptCount;

        stats.textureBytes = GetTotalTextureVRAMBytes();
        stats.soundBytes   = GetTotalSoundMemoryBytes();
        stats.musicBytes   = GetTotalMusicMemoryBytes();
        stats.fontBytes    = GetTotalFontVRAMBytes();
        stats.prefabBytes  = GetTotalPrefabMemoryBytes();
        stats.scriptBytes  = GetTotalScriptMemoryBytes();
        stats.totalBytes   = stats.textureBytes + stats.soundBytes + stats.musicBytes +
                             stats.fontBytes + stats.prefabBytes + stats.scriptBytes;

        stats.totalLoadCalls = s_totalLoadCalls;
        stats.totalCacheHits = s_totalCacheHits;
        return stats;
    }

} // namespace Talos::Scene
