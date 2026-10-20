// Unit tests for the unified central AssetManager (textures, audio, fonts, prefabs, scripts)
#include "tests/test_framework.h"
#include "scene/asset_manager.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "renderer/renderer2d.h"
#include "audio/audio_system.h"
#include <cstdint>
#include <filesystem>
#include <fstream>

using namespace Talos::Scene;
using namespace Talos::Renderer;
using namespace Talos::Audio;
namespace fs = std::filesystem;

namespace {
    const std::string kTempDir = "build/test_asset_mgr_tmp";

    void CleanTempDir() {
        std::error_code ec;
        fs::remove_all(kTempDir, ec);
        fs::create_directories(kTempDir, ec);
    }

    // Writes a minimal valid 16-bit mono PCM WAV file for headless sound/music testing
    void WriteTinyWav(const std::string& filePath) {
        std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
        const std::uint32_t sampleRate = 8000;
        const std::uint32_t numSamples = 800;
        const std::uint32_t dataBytes  = numSamples * 2;
        const std::uint32_t riffSize   = 36 + dataBytes;
        const std::uint32_t fmtSize    = 16;
        const std::uint16_t audioFmt   = 1;
        const std::uint16_t channels   = 1;
        const std::uint32_t byteRate   = sampleRate * 2;
        const std::uint16_t blockAlign = 2;
        const std::uint16_t bits       = 16;

        out.write("RIFF", 4);
        out.write(reinterpret_cast<const char*>(&riffSize), 4);
        out.write("WAVE", 4);
        out.write("fmt ", 4);
        out.write(reinterpret_cast<const char*>(&fmtSize), 4);
        out.write(reinterpret_cast<const char*>(&audioFmt), 2);
        out.write(reinterpret_cast<const char*>(&channels), 2);
        out.write(reinterpret_cast<const char*>(&sampleRate), 4);
        out.write(reinterpret_cast<const char*>(&byteRate), 4);
        out.write(reinterpret_cast<const char*>(&blockAlign), 2);
        out.write(reinterpret_cast<const char*>(&bits), 2);
        out.write("data", 4);
        out.write(reinterpret_cast<const char*>(&dataBytes), 4);
        std::int16_t sample = 0;
        for (std::uint32_t i = 0; i < numSamples; ++i) {
            out.write(reinterpret_cast<const char*>(&sample), 2);
        }
    }
}

TEST(AssetManager_PathNormalizationAndTypeDetection) {
    CHECK_EQ(AssetManager::NormalizePath("  ./examples\\assets\\textures\\crate.png  "),
             std::string("examples/assets/textures/crate.png"));
    CHECK_EQ(AssetManager::NormalizePath("\"examples//prefabs\\\\crate.prefab\""),
             std::string("examples/prefabs/crate.prefab"));
    CHECK_EQ(AssetManager::NormalizePath("   "), std::string(""));

    CHECK(AssetManager::DetectType("hero.PNG") == AssetType::Texture);
    CHECK(AssetManager::DetectType("tile.bmp") == AssetType::Texture);
    CHECK(AssetManager::DetectType("bg.jpg") == AssetType::Texture);
    CHECK(AssetManager::DetectType("photo.jpeg") == AssetType::Texture);
    CHECK(AssetManager::DetectType("jump.wav") == AssetType::Sound);
    CHECK(AssetManager::DetectType("sfx_coin.OGG") == AssetType::Sound);
    CHECK(AssetManager::DetectType("ambient.OGG") == AssetType::Music);
    CHECK(AssetManager::DetectType("theme.mp3") == AssetType::Music);
    CHECK(AssetManager::DetectType("ui_font.ttf") == AssetType::Font);
    CHECK(AssetManager::DetectType("display.OTF") == AssetType::Font);
    CHECK(AssetManager::DetectType("enemy.prefab") == AssetType::Prefab);
    CHECK(AssetManager::DetectType("controller.lua") == AssetType::Script);
    CHECK(AssetManager::DetectType("readme.txt") == AssetType::Unknown);
}

TEST(AssetManager_TextureSoundMusicFontPrefabScriptLifecycle) {
    CleanTempDir();
    AssetManager::Init();
    CHECK(AssetManager::IsInitialized());
    AssetManager::UnloadAll();

    // 1. Texture load, reuse (cache hit), and query
    const std::string texPath = "examples/assets/textures/crate.png";
    CHECK(!AssetManager::HasTexture(texPath));
    CHECK(AssetManager::LoadTexture(texPath));
    CHECK(AssetManager::HasTexture(texPath));
    CHECK(AssetManager::Has(texPath));

    AssetRecord texRec1 = AssetManager::GetRecord(texPath);
    CHECK(texRec1.loaded);
    CHECK(texRec1.type == AssetType::Texture);
    CHECK_EQ(texRec1.loadCount, 1u);
    CHECK_EQ(texRec1.reuseCount, 0u);
    CHECK(texRec1.memoryBytes > 0);

    // Second load reuses cached texture and increments reuseCount
    CHECK(AssetManager::LoadTexture("  ./examples\\assets\\textures\\crate.png "));
    AssetRecord texRec2 = AssetManager::GetRecord(texPath);
    CHECK(texRec2.loaded);
    CHECK_EQ(texRec2.loadCount, 2u);
    CHECK_EQ(texRec2.reuseCount, 1u);

    // 2. Sound & Music load, reuse, and query
    const std::string sfxPath = "examples/assets/audio/coin_pickup.wav";
    const std::string bgmPath = kTempDir + "/bgm_theme.wav";
    WriteTinyWav(bgmPath);

    CHECK(AssetManager::LoadSound(sfxPath));
    CHECK(AssetManager::LoadSound(sfxPath)); // cache hit
    CHECK(AssetManager::HasSound(sfxPath));

    CHECK(AssetManager::LoadMusic(bgmPath));
    CHECK(AssetManager::LoadMusic(bgmPath)); // cache hit
    CHECK(AssetManager::HasMusic(bgmPath));

    // 3. Font load, reuse, and query (with valid TrueType 0x00010000 SFNT header)
    const std::string fontPath = kTempDir + "/pixel_ui.ttf";
    {
        std::ofstream out(fontPath, std::ios::binary);
        const unsigned char sfntHeader[16] = {
            0x00, 0x01, 0x00, 0x00, // sfntVersion = 1.0 (TrueType)
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };
        out.write(reinterpret_cast<const char*>(sfntHeader), sizeof(sfntHeader));
    }
    CHECK(!AssetManager::HasFont(fontPath));
    CHECK(AssetManager::Load(fontPath)); // Auto-detects AssetType::Font
    CHECK(AssetManager::HasFont(fontPath));
    CHECK(AssetManager::LoadFont(fontPath)); // Cache hit
    AssetRecord fontRec = AssetManager::GetRecord(fontPath);
    CHECK(fontRec.loaded);
    CHECK(fontRec.type == AssetType::Font);
    CHECK_EQ(fontRec.loadCount, 2u);
    CHECK_EQ(fontRec.reuseCount, 1u);

    // 4. Prefab save/load, cache reuse, and instantiation
    const std::string prefabPath = kTempDir + "/turret.prefab";
    Entity turretTemplate;
    turretTemplate.id = 42;
    turretTemplate.name = "PlasmaTurret";
    turretTemplate.tag = "Defense";
    turretTemplate.transform.x = 250.0f;
    turretTemplate.transform.y = 180.0f;
    turretTemplate.sprite.enabled = true;
    turretTemplate.sprite.texturePath = texPath;
    CHECK(SceneSerializer::SavePrefab(turretTemplate, prefabPath));
    // SavePrefab automatically caches the prefab template in AssetManager
    CHECK(AssetManager::HasPrefab(prefabPath));

    const Entity* cachedPrefab = AssetManager::GetPrefab(prefabPath);
    CHECK(cachedPrefab != nullptr);
    if (cachedPrefab) {
        CHECK_EQ(cachedPrefab->name, std::string("PlasmaTurret"));
        CHECK_EQ(cachedPrefab->tag, std::string("Defense"));
    }

    // Verify InstantiatePrefab reuses the cached prefab
    CHECK(AssetManager::LoadPrefab(prefabPath));
    Scene targetScene;
    Entity* spawned = SceneSerializer::InstantiatePrefab(targetScene, prefabPath);
    CHECK(spawned != nullptr);
    if (spawned) {
        CHECK_EQ(spawned->name, std::string("PlasmaTurret"));
        CHECK_NEAR(spawned->transform.x, 250.0f, 0.01f);
    }

    // 5. Script (.lua) load, reuse, and source retrieval
    const std::string scriptPath = kTempDir + "/player_controller.lua";
    const std::string luaCode = "-- Player movement script\nfunction on_update(dt)\n    -- move\nend\n";
    {
        std::ofstream out(scriptPath, std::ios::binary);
        out << luaCode;
    }
    CHECK(!AssetManager::HasScript(scriptPath));
    CHECK(AssetManager::Load(scriptPath)); // Auto-detects AssetType::Script
    CHECK(AssetManager::HasScript(scriptPath));
    CHECK_EQ(AssetManager::GetScriptSource(scriptPath), luaCode);
    CHECK(AssetManager::LoadScript(scriptPath)); // Cache hit

    // 6. Verify aggregate AssetStats across all 6 resource categories
    AssetStats stats = AssetManager::GetStats();
    CHECK_EQ(stats.textureCount, static_cast<std::size_t>(1));
    CHECK_EQ(stats.soundCount,   static_cast<std::size_t>(1));
    CHECK_EQ(stats.musicCount,   static_cast<std::size_t>(1));
    CHECK_EQ(stats.fontCount,    static_cast<std::size_t>(1));
    CHECK_EQ(stats.prefabCount,  static_cast<std::size_t>(1));
    CHECK_EQ(stats.scriptCount,  static_cast<std::size_t>(1));
    CHECK_EQ(stats.totalCount,   static_cast<std::size_t>(6));
    CHECK(stats.totalBytes > 0);
    CHECK(stats.totalCacheHits >= 5);

    // 7. Unload individual resources & UnloadByType
    AssetManager::UnloadFont(fontPath);
    CHECK(!AssetManager::HasFont(fontPath));
    AssetManager::UnloadScript(scriptPath);
    CHECK(!AssetManager::HasScript(scriptPath));
    CHECK_EQ(AssetManager::GetCachedScriptCount(), static_cast<std::size_t>(0));
    AssetManager::UnloadByType(AssetType::Prefab);
    CHECK(!AssetManager::HasPrefab(prefabPath));

    AssetManager::Shutdown();
    CHECK(!AssetManager::IsInitialized());
    CHECK_EQ(AssetManager::GetStats().totalCount, static_cast<std::size_t>(0));

    std::error_code ec;
    fs::remove_all(kTempDir, ec);
}

TEST(AssetManager_PreloadAndUnloadUnusedSceneAssets) {
    CleanTempDir();
    AssetManager::Init();
    AssetManager::UnloadAll();

    // Load an extra texture and sound that will NOT be used by the scene
    const std::string unusedTex = "examples/assets/textures/brick.png";
    const std::string unusedSfx = kTempDir + "/unused_sfx.wav";
    WriteTinyWav(unusedSfx);

    CHECK(AssetManager::LoadTexture(unusedTex));
    CHECK(AssetManager::LoadSound(unusedSfx));

    // Build a scene that references crate.png and coin_pickup.wav
    const std::string activeTex = "examples/assets/textures/crate.png";
    const std::string activeSfx = "examples/assets/audio/coin_pickup.wav";
    Scene scene;
    Entity& player = scene.CreateEntity("Player");
    player.sprite.enabled = true;
    player.sprite.texturePath = activeTex;
    player.audio.enabled = true;
    player.audio.audioPath = activeSfx;
    player.audio.isMusic = false;

    std::size_t preloaded = AssetManager::PreloadSceneAssets(scene);
    CHECK_EQ(preloaded, static_cast<std::size_t>(2));
    CHECK(AssetManager::HasTexture(activeTex));
    CHECK(AssetManager::HasSound(activeSfx));
    CHECK(AssetManager::HasTexture(unusedTex));
    CHECK(AssetManager::HasSound(unusedSfx));

    // UnloadUnusedSceneAssets should release brick.png and unused_sfx.wav while keeping Player's assets
    std::size_t pruned = AssetManager::UnloadUnusedSceneAssets(scene);
    CHECK_EQ(pruned, static_cast<std::size_t>(2));
    CHECK(AssetManager::HasTexture(activeTex));
    CHECK(AssetManager::HasSound(activeSfx));
    CHECK(!AssetManager::HasTexture(unusedTex));
    CHECK(!AssetManager::HasSound(unusedSfx));

    AssetManager::Shutdown();
    std::error_code ec;
    fs::remove_all(kTempDir, ec);
}

TEST_MAIN()
