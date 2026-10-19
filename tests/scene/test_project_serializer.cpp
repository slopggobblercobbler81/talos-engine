// Unit tests for ProjectSerializer: project.talos round-trip, clamping, legacy keys, and workflow
#include "tests/test_framework.h"
#include "scene/project_serializer.h"
#include "scene/scene_serializer.h"
#include "audio/audio_system.h"
#include "core/input.h"
#include "core/input_map.h"
#include <filesystem>
#include <fstream>

using namespace Talos::Scene;
using namespace Talos::Audio;
using namespace Talos::Core;
namespace fs = std::filesystem;

namespace {

    fs::path TempDir() {
        fs::path dir = fs::temp_directory_path() / "talos_project_serializer_tests";
        fs::create_directories(dir);
        return dir;
    }

    void WriteText(const std::string& path, const std::string& text) {
        std::ofstream out(path, std::ios::trunc);
        out << text;
    }

}

TEST(MakeDefaultConfig_PopulatesDefaultsAndBuiltInActions) {
    ProjectConfig cfg = ProjectSerializer::MakeDefaultConfig();
    CHECK_EQ(cfg.name, std::string("Talos Starter Project"));
    CHECK_EQ(cfg.version, std::string("0.1.0"));
    CHECK_EQ(cfg.windowWidth, 1360);
    CHECK_EQ(cfg.windowHeight, 780);
    CHECK_EQ(cfg.targetFps, 60);
    CHECK(!cfg.fullscreen);
    CHECK(cfg.resizable);
    CHECK_EQ(cfg.startScene, std::string("examples/scenes/main.talsc"));
    CHECK_NEAR(cfg.masterVolume, 1.0f, 1e-4f);
    CHECK_NEAR(cfg.sfxVolume, 1.0f, 1e-4f);
    CHECK_NEAR(cfg.musicVolume, 1.0f, 1e-4f);
    CHECK(cfg.inputActions.size() >= 6);
}

TEST(RoundTrip_PreservesProjectAudioAndInputMapData) {
    ProjectConfig original = ProjectSerializer::MakeDefaultConfig();
    original.name = "Cyber Runner 2099";
    original.version = "1.2.3";
    original.windowWidth = 1920;
    original.windowHeight = 1080;
    original.targetFps = 144;
    original.fullscreen = true;
    original.resizable = false;
    original.startScene = "scenes/level_01.talsc";
    original.masterVolume = 0.85f;
    original.sfxVolume = 0.65f;
    original.musicVolume = 0.40f;

    original.inputActions.clear();
    original.inputActions.push_back({ "move_left",  { Key::A, Key::Left } });
    original.inputActions.push_back({ "move_right", { Key::D, Key::Right } });
    original.inputActions.push_back({ "jump",       { Key::Space, Key::W } });
    original.inputActions.push_back({ "dash",       { Key::LeftShift } });

    const std::string path = (TempDir() / "roundtrip_project.talos").string();
    CHECK(ProjectSerializer::Save(original, path));

    // Reset subsystems to defaults before loading to verify ApplyToEngine
    AudioSystem::ResetDefaultVolumes();
    InputMap::InitDefaults();

    ProjectConfig loaded;
    CHECK(ProjectSerializer::Load(loaded, path, true));

    CHECK_EQ(loaded.name, std::string("Cyber Runner 2099"));
    CHECK_EQ(loaded.version, std::string("1.2.3"));
    CHECK_EQ(loaded.windowWidth, 1920);
    CHECK_EQ(loaded.windowHeight, 1080);
    CHECK_EQ(loaded.targetFps, 144);
    CHECK(loaded.fullscreen);
    CHECK(!loaded.resizable);
    CHECK_EQ(loaded.startScene, std::string("scenes/level_01.talsc"));
    CHECK_NEAR(loaded.masterVolume, 0.85f, 1e-4f);
    CHECK_NEAR(loaded.sfxVolume, 0.65f, 1e-4f);
    CHECK_NEAR(loaded.musicVolume, 0.40f, 1e-4f);

    // Verify subsystems received the loaded configuration
    CHECK_NEAR(AudioSystem::GetMasterVolume(), 0.85f, 1e-4f);
    CHECK_NEAR(AudioSystem::GetSFXVolume(), 0.65f, 1e-4f);
    CHECK_NEAR(AudioSystem::GetMusicBusVolume(), 0.40f, 1e-4f);

    CHECK_EQ(loaded.inputActions.size(), static_cast<size_t>(4));
    const InputAction* jumpAct = InputMap::FindAction("jump");
    CHECK(jumpAct != nullptr);
    if (jumpAct) {
        CHECK_EQ(jumpAct->keys.size(), static_cast<size_t>(2));
        CHECK_EQ(static_cast<int>(jumpAct->keys[0]), static_cast<int>(Key::Space));
        CHECK_EQ(static_cast<int>(jumpAct->keys[1]), static_cast<int>(Key::W));
    }

    const InputAction* dashAct = InputMap::FindAction("dash");
    CHECK(dashAct != nullptr);
    if (dashAct) {
        CHECK_EQ(dashAct->keys.size(), static_cast<size_t>(1));
        CHECK_EQ(static_cast<int>(dashAct->keys[0]), static_cast<int>(Key::LeftShift));
    }

    AudioSystem::ResetDefaultVolumes();
    InputMap::InitDefaults();
}

TEST(Load_ClampsOutOfRangeValuesAndAcceptsLegacyAliases) {
    const std::string path = (TempDir() / "clamped_project.talos").string();
    WriteText(path,
        "# Test legacy aliases & clamping\n"
        "[Project]\n"
        "title =   \n"
        "version =   \n"
        "width = 120\n"
        "height = 99999\n"
        "fps = 5\n"
        "fullscreen = true\n"
        "resizable = 0\n"
        "initial_scene =   \n"
        "\n"
        "[Audio]\n"
        "master = 2.5\n"
        "sfx = -1.0\n"
        "music = 0.55\n"
        "\n"
        "[InputMap]\n"
        "fire_laser = Space, Enter, InvalidKeyName, Space\n"
    );

    ProjectConfig loaded;
    CHECK(ProjectSerializer::Load(loaded, path, false));

    CHECK_EQ(loaded.name, std::string("Untitled Talos Project"));
    CHECK_EQ(loaded.version, std::string("0.1.0"));
    CHECK_EQ(loaded.windowWidth, 320);
    CHECK_EQ(loaded.windowHeight, 4320);
    CHECK_EQ(loaded.targetFps, 15);
    CHECK(loaded.fullscreen);
    CHECK(!loaded.resizable);
    CHECK_EQ(loaded.startScene, std::string("examples/scenes/main.talsc"));
    CHECK_NEAR(loaded.masterVolume, 1.0f, 1e-4f);
    CHECK_NEAR(loaded.sfxVolume, 0.0f, 1e-4f);
    CHECK_NEAR(loaded.musicVolume, 0.55f, 1e-4f);

    CHECK_EQ(loaded.inputActions.size(), static_cast<size_t>(1));
    CHECK_EQ(loaded.inputActions[0].name, std::string("fire_laser"));
    // Duplicate 'Space' and unknown 'InvalidKeyName' are filtered out
    CHECK_EQ(loaded.inputActions[0].keys.size(), static_cast<size_t>(2));
}

TEST(Load_MissingOrEmptyFileFailsGracefully) {
    ProjectConfig cfg;
    CHECK(!ProjectSerializer::Load(cfg, (TempDir() / "does_not_exist.talos").string(), false));

    const std::string emptyPath = (TempDir() / "empty_project.talos").string();
    WriteText(emptyPath, "   \n# comment only\n");
    CHECK(!ProjectSerializer::Load(cfg, emptyPath, false));
}

TEST(Workflow_CreateSaveAndOpenProjectEndToEnd) {
    fs::path workDir = TempDir() / "workflow_project";
    fs::remove_all(workDir);
    fs::create_directories(workDir);

    const std::string projectPath = (workDir / "project.talos").string();

    // 1. Create a brand-new project with starter entities
    ProjectConfig createCfg = ProjectSerializer::MakeDefaultConfig();
    createCfg.name = "My Adventure Game";
    createCfg.windowWidth = 1600;
    createCfg.windowHeight = 900;
    createCfg.targetFps = 120;
    createCfg.startScene = "scenes/start_world.talsc";

    Scene createdScene;
    CHECK(ProjectSerializer::CreateNewProject(createCfg, createdScene, projectPath, true));
    CHECK(fs::exists(projectPath));
    CHECK(fs::exists(workDir / "scenes" / "start_world.talsc"));
    CHECK_EQ(createdScene.GetEntities().size(), static_cast<size_t>(5));
    CHECK(createdScene.FindEntity("Player") != nullptr);

    // 2. Modify the scene and subsystem settings, then SaveProject
    Entity& boss = createdScene.CreateEntity("BossDragon", 800.0f, 200.0f);
    boss.tag = "Enemy";
    AudioSystem::SetMasterVolume(0.70f);
    AudioSystem::SetSFXVolume(0.50f);
    AudioSystem::SetMusicBusVolume(0.30f);
    InputMap::AddAction("special_attack");
    InputMap::BindKey("special_attack", Key::E);

    std::string resolvedScenePath = ProjectSerializer::ResolveScenePath(projectPath, createCfg.startScene);
    CHECK(ProjectSerializer::SaveProject(createCfg, &createdScene, projectPath, resolvedScenePath, true));

    // 3. Reset runtime state and OpenProject from disk into a fresh Scene
    AudioSystem::ResetDefaultVolumes();
    InputMap::InitDefaults();

    ProjectConfig openedCfg;
    Scene openedScene;
    CHECK(ProjectSerializer::OpenProject(openedCfg, openedScene, projectPath));

    CHECK_EQ(openedCfg.name, std::string("My Adventure Game"));
    CHECK_EQ(openedCfg.windowWidth, 1600);
    CHECK_EQ(openedCfg.windowHeight, 900);
    CHECK_EQ(openedCfg.targetFps, 120);
    CHECK_EQ(openedCfg.startScene, std::string("scenes/start_world.talsc"));
    CHECK_NEAR(openedCfg.masterVolume, 0.70f, 1e-4f);
    CHECK_NEAR(openedCfg.sfxVolume, 0.50f, 1e-4f);
    CHECK_NEAR(openedCfg.musicVolume, 0.30f, 1e-4f);
    CHECK_NEAR(AudioSystem::GetMasterVolume(), 0.70f, 1e-4f);
    CHECK_NEAR(AudioSystem::GetSFXVolume(), 0.50f, 1e-4f);
    CHECK_NEAR(AudioSystem::GetMusicBusVolume(), 0.30f, 1e-4f);

    const InputAction* specialAct = InputMap::FindAction("special_attack");
    CHECK(specialAct != nullptr);
    if (specialAct) {
        CHECK_EQ(specialAct->keys.size(), static_cast<size_t>(1));
        CHECK_EQ(static_cast<int>(specialAct->keys[0]), static_cast<int>(Key::E));
    }

    CHECK_EQ(openedScene.GetEntities().size(), static_cast<size_t>(6));
    const Entity* loadedBoss = openedScene.FindEntity("BossDragon");
    CHECK(loadedBoss != nullptr);
    if (loadedBoss) {
        CHECK_EQ(loadedBoss->tag, std::string("Enemy"));
        CHECK_NEAR(loadedBoss->transform.x, 800.0f, 1e-3f);
        CHECK_NEAR(loadedBoss->transform.y, 200.0f, 1e-3f);
    }

    AudioSystem::ResetDefaultVolumes();
    InputMap::InitDefaults();
}

TEST_MAIN()
