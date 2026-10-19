// Unified project configuration (ProjectConfig) and project.talos workflow (ProjectSerializer)
#include "scene/project_serializer.h"
#include "scene/scene_serializer.h"
#include "audio/audio_system.h"
#include "core/input.h"
#include "core/logger.h"
#include "core/window.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Talos::Scene {

    namespace {

        std::string Trim(const std::string& str) {
            const char* whitespace = " \t\r\n";
            size_t first = str.find_first_not_of(whitespace);
            if (first == std::string::npos) {
                return "";
            }
            size_t last = str.find_last_not_of(whitespace);
            return str.substr(first, (last - first + 1));
        }

        std::string ToLower(std::string s) {
            for (char& c : s) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return s;
        }

        bool ParseBoolValue(const std::string& val) {
            std::string lower = ToLower(Trim(val));
            if (lower == "true" || lower == "yes" || lower == "on") {
                return true;
            }
            if (lower == "false" || lower == "no" || lower == "off") {
                return false;
            }
            return std::atoi(lower.c_str()) != 0;
        }

        enum class ProjectSection {
            Project,
            Audio,
            InputMap,
            Unknown
        };

    } // anonymous namespace

    ProjectConfig ProjectSerializer::MakeDefaultConfig() {
        ProjectConfig config;
        if (Core::InputMap::GetActions().empty()) {
            Core::InputMap::InitDefaults();
        }
        config.inputActions = Core::InputMap::GetActions();
        return config;
    }

    void ProjectSerializer::SanitizeConfig(ProjectConfig& inOutConfig) {
        inOutConfig.name = Trim(inOutConfig.name);
        if (inOutConfig.name.empty()) {
            inOutConfig.name = "Untitled Talos Project";
        }

        inOutConfig.version = Trim(inOutConfig.version);
        if (inOutConfig.version.empty()) {
            inOutConfig.version = "0.1.0";
        }

        inOutConfig.windowWidth  = std::max(320, std::min(7680, inOutConfig.windowWidth));
        inOutConfig.windowHeight = std::max(240, std::min(4320, inOutConfig.windowHeight));
        inOutConfig.targetFps    = std::max(15,  std::min(240,  inOutConfig.targetFps));

        inOutConfig.startScene = Trim(inOutConfig.startScene);
        if (inOutConfig.startScene.empty()) {
            inOutConfig.startScene = "examples/scenes/main.talsc";
        }

        inOutConfig.masterVolume = std::max(0.0f, std::min(1.0f, inOutConfig.masterVolume));
        inOutConfig.sfxVolume    = std::max(0.0f, std::min(1.0f, inOutConfig.sfxVolume));
        inOutConfig.musicVolume  = std::max(0.0f, std::min(1.0f, inOutConfig.musicVolume));
    }

    std::string ProjectSerializer::ResolveScenePath(const std::string& projectFilePath,
                                                    const std::string& scenePath) {
        std::string trimmedScene = Trim(scenePath);
        if (trimmedScene.empty()) {
            trimmedScene = "examples/scenes/main.talsc";
        }

        std::filesystem::path sp(trimmedScene);
        if (sp.is_absolute()) {
            return sp.generic_string();
        }

        std::filesystem::path pp(projectFilePath);
        if (!pp.has_parent_path()) {
            return sp.generic_string();
        }

        std::filesystem::path parentDir = pp.parent_path();
        if (parentDir.empty() || parentDir == ".") {
            return sp.generic_string();
        }

        std::error_code ec;
        if (std::filesystem::exists(sp, ec)) {
            return sp.generic_string();
        }

        std::string parentStr = parentDir.generic_string();
        std::string sceneStr  = sp.generic_string();
        if (sceneStr.rfind(parentStr + "/", 0) == 0) {
            return sceneStr;
        }

        return (parentDir / sp).lexically_normal().generic_string();
    }

    void ProjectSerializer::CaptureFromEngine(ProjectConfig& inOutConfig) {
        inOutConfig.masterVolume = Audio::AudioSystem::GetMasterVolume();
        inOutConfig.sfxVolume    = Audio::AudioSystem::GetSFXVolume();
        inOutConfig.musicVolume  = Audio::AudioSystem::GetMusicBusVolume();

        if (Core::InputMap::GetActions().empty()) {
            Core::InputMap::InitDefaults();
        }
        inOutConfig.inputActions = Core::InputMap::GetActions();
    }

    void ProjectSerializer::ApplyToEngine(const ProjectConfig& config) {
        Audio::AudioSystem::SetMasterVolume(config.masterVolume);
        Audio::AudioSystem::SetSFXVolume(config.sfxVolume);
        Audio::AudioSystem::SetMusicBusVolume(config.musicVolume);

        if (!config.inputActions.empty()) {
            Core::InputMap::GetActionsMutable() = config.inputActions;
        } else if (Core::InputMap::GetActions().empty()) {
            Core::InputMap::InitDefaults();
        }

        Core::Window::SetTargetFrameRate(config.targetFps);
        std::string windowTitle = "Talos Engine - " + config.name + " [" + config.startScene + "]";
        Core::Window::SetTitle(windowTitle.c_str());
    }

    void ProjectSerializer::PopulateDefaultStarterScene(Scene& outScene) {
        outScene.Clear();

        // Title Banner Entity (Overlay Layer)
        Entity& title = outScene.CreateEntity("TitleBanner", 260.0f, 75.0f);
        title.sprite.enabled = false;
        title.sprite.sortingLayer = SortingLayer::Overlay;
        title.sprite.zOrder = 20;
        title.text.enabled = true;
        title.text.content = "Welcome to Talos Engine!";
        title.text.fontSize = 28;
        title.text.color = { 240, 240, 245, 255 };

        // Player Entity: Uses 4-frame animated sheet texture and is controllable with WASD/Arrows in Play Mode
        Entity& player = outScene.CreateEntity("Player", 360.0f, 340.0f);
        player.tag = "Player";
        player.transform.width = 72.0f;
        player.transform.height = 72.0f;
        player.sprite.enabled = true;
        player.sprite.texturePath = "examples/assets/textures/sheet.png";
        player.sprite.color = { 255, 255, 255, 255 };
        player.sprite.sortingLayer = SortingLayer::Characters;
        player.sprite.zOrder = 10;
        player.sprite.sourceW = 64.0f;
        player.sprite.sourceH = 64.0f;
        player.sprite.frameCount = 4;
        player.sprite.currentFrame = 0;
        player.sprite.fps = 8.0f;
        player.sprite.loop = true;
        player.sprite.isPlaying = true;
        player.collider.enabled = true;
        player.collider.shape = Physics::ColliderShape::Box;
        player.movement.enabled = true;
        player.movement.playerControlled = true;
        player.movement.speed = 320.0f;
        player.movement.mass = 2.0f;
        player.camera.enabled = true;
        player.camera.isPrimary = true;
        player.camera.followTarget = true;
        player.camera.smoothFollow = true;
        player.camera.targetName = "Player";
        player.camera.followSpeed = 8.0f;
        player.camera.zoom = 1.0f;
        player.text.enabled = true;
        player.text.content = "Player";
        player.text.fontSize = 18;
        player.text.offsetY = -24.0f;

        // Wooden Crate Entity
        Entity& crate = outScene.CreateEntity("WoodenCrate", 530.0f, 430.0f);
        crate.tag = "Obstacle";
        crate.transform.width = 68.0f;
        crate.transform.height = 68.0f;
        crate.sprite.enabled = true;
        crate.sprite.texturePath = "examples/assets/textures/crate.png";
        crate.sprite.color = { 255, 255, 255, 255 };
        crate.sprite.sortingLayer = SortingLayer::Scenery;
        crate.sprite.zOrder = 0;
        crate.collider.enabled = true;
        crate.collider.shape = Physics::ColliderShape::Box;
        crate.collider.isStatic = true;
        crate.text.enabled = true;
        crate.text.content = "Crate";
        crate.text.fontSize = 16;
        crate.text.offsetY = -22.0f;

        // Solid Wall Entity
        Entity& wall = outScene.CreateEntity("BrickWall", 700.0f, 230.0f);
        wall.tag = "Wall";
        wall.transform.width = 110.0f;
        wall.transform.height = 280.0f;
        wall.sprite.enabled = true;
        wall.sprite.texturePath = "examples/assets/textures/brick.png";
        wall.sprite.color = { 255, 255, 255, 255 };
        wall.sprite.sortingLayer = SortingLayer::Scenery;
        wall.sprite.zOrder = 5;
        wall.collider.enabled = true;
        wall.collider.shape = Physics::ColliderShape::Box;
        wall.collider.isStatic = true;
        wall.text.enabled = true;
        wall.text.content = "Brick Wall";
        wall.text.fontSize = 18;
        wall.text.offsetY = -24.0f;

        // Gold Coin Entity
        Entity& coin = outScene.CreateEntity("GoldCoin", 540.0f, 250.0f);
        coin.tag = "Coin";
        coin.transform.width = 56.0f;
        coin.transform.height = 56.0f;
        coin.sprite.enabled = true;
        coin.sprite.texturePath = "examples/assets/textures/coin.png";
        coin.sprite.color = { 255, 255, 255, 255 };
        coin.sprite.sortingLayer = SortingLayer::Scenery;
        coin.sprite.zOrder = 10;
        coin.collider.enabled = true;
        coin.collider.shape = Physics::ColliderShape::Circle;
        coin.collider.radius = 0.0f;
        coin.collider.restitution = 0.85f;
        coin.collider.isStatic = false;
        coin.movement.enabled = true;
        coin.movement.playerControlled = false;
        coin.movement.mass = 1.0f;
        coin.text.enabled = true;
        coin.text.content = "Coin";
        coin.text.fontSize = 16;
        coin.text.offsetY = -22.0f;
    }

    bool ProjectSerializer::Save(const ProjectConfig& config, const std::string& filePath) {
        if (filePath.empty()) {
            TALOS_LOG_ERROR("ProjectSerializer::Save failed: empty file path.");
            return false;
        }

        ProjectConfig sanitized = config;
        SanitizeConfig(sanitized);

        std::filesystem::path pathObj(filePath);
        if (pathObj.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(pathObj.parent_path(), ec);
        }

        std::ofstream out(filePath, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            TALOS_LOG_ERROR("Failed to open project file for writing: '" + filePath + "'");
            return false;
        }

        out << "# Talos Engine Project File (v1)\n";
        out << "# Unified project configuration, initial scene, audio buses, and input map\n\n";
        out << std::fixed << std::setprecision(2);

        out << "[Project]\n";
        out << "name = " << sanitized.name << "\n";
        out << "version = " << sanitized.version << "\n";
        out << "windowWidth = " << sanitized.windowWidth << "\n";
        out << "windowHeight = " << sanitized.windowHeight << "\n";
        out << "targetFps = " << sanitized.targetFps << "\n";
        out << "fullscreen = " << (sanitized.fullscreen ? 1 : 0) << "\n";
        out << "resizable = " << (sanitized.resizable ? 1 : 0) << "\n";
        out << "startScene = " << sanitized.startScene << "\n\n";

        out << "[Audio]\n";
        out << "masterVolume = " << sanitized.masterVolume << "\n";
        out << "sfxVolume = " << sanitized.sfxVolume << "\n";
        out << "musicVolume = " << sanitized.musicVolume << "\n\n";

        out << "[InputMap]\n";
        const std::vector<Core::InputAction>* actionsToWrite = &sanitized.inputActions;
        if (actionsToWrite->empty()) {
            if (Core::InputMap::GetActions().empty()) {
                Core::InputMap::InitDefaults();
            }
            actionsToWrite = &Core::InputMap::GetActions();
        }

        for (const auto& action : *actionsToWrite) {
            if (action.name.empty()) continue;
            out << action.name << " = ";
            for (size_t i = 0; i < action.keys.size(); ++i) {
                out << Core::Input::GetKeyToken(action.keys[i]);
                if (i + 1 < action.keys.size()) {
                    out << ", ";
                }
            }
            out << "\n";
        }

        TALOS_LOG_INFO("Project saved to '" + filePath + "' (title='" + sanitized.name +
                       "', startScene='" + sanitized.startScene + "')");
        return true;
    }

    bool ProjectSerializer::Load(ProjectConfig& outConfig,
                                 const std::string& filePath,
                                 bool applyToSubsystems) {
        std::ifstream in(filePath, std::ios::in);
        if (!in.is_open()) {
            TALOS_LOG_WARN("Project file not found or could not be opened: '" + filePath + "'");
            return false;
        }

        outConfig = ProjectConfig{};
        outConfig.inputActions.clear();

        ProjectSection currentSection = ProjectSection::Project;
        std::string line;
        int fileLineNumber = 0;
        bool parsedAnyEntry = false;

        auto upsertInputAction = [&](const std::string& actionName, const std::string& keysPart) {
            std::string cleanName = Trim(actionName);
            if (cleanName.empty()) return;
            parsedAnyEntry = true;

            Core::InputAction parsedAction;
            parsedAction.name = cleanName;

            std::stringstream ss(keysPart);
            std::string keyToken;
            while (std::getline(ss, keyToken, ',')) {
                std::string trimmedToken = Trim(keyToken);
                if (trimmedToken.empty()) continue;
                Core::Key parsedKey = Core::Key::Unknown;
                if (Core::Input::TryParseKeyToken(trimmedToken, parsedKey) &&
                    parsedKey != Core::Key::Unknown) {
                    if (std::find(parsedAction.keys.begin(), parsedAction.keys.end(), parsedKey) ==
                        parsedAction.keys.end()) {
                        parsedAction.keys.push_back(parsedKey);
                    }
                } else {
                    Core::Logger::Write(Core::LogLevel::Warning,
                                        "Unknown key token '" + trimmedToken + "' in action '" + cleanName + "'",
                                        filePath.c_str(), fileLineNumber);
                }
            }

            for (auto& existing : outConfig.inputActions) {
                if (existing.name == cleanName) {
                    existing.keys = std::move(parsedAction.keys);
                    return;
                }
            }
            outConfig.inputActions.push_back(std::move(parsedAction));
        };

        while (std::getline(in, line)) {
            ++fileLineNumber;
            std::string trimmed = Trim(line);
            if (trimmed.empty() || trimmed[0] == '#') {
                continue;
            }

            if (trimmed.front() == '[' && trimmed.back() == ']') {
                std::string secName = ToLower(Trim(trimmed.substr(1, trimmed.size() - 2)));
                if (secName == "project" || secName == "general" || secName == "window" || secName == "application") {
                    currentSection = ProjectSection::Project;
                    parsedAnyEntry = true;
                } else if (secName == "audio" || secName == "sound") {
                    currentSection = ProjectSection::Audio;
                    parsedAnyEntry = true;
                } else if (secName == "inputmap" || secName == "input_map" || secName == "input") {
                    currentSection = ProjectSection::InputMap;
                    parsedAnyEntry = true;
                } else {
                    currentSection = ProjectSection::Unknown;
                    Core::Logger::Write(Core::LogLevel::Warning,
                                        "Unknown project section '[" + secName + "]'",
                                        filePath.c_str(), fileLineNumber);
                }
                continue;
            }

            size_t eqPos = trimmed.find('=');
            if (eqPos == std::string::npos) {
                Core::Logger::Write(Core::LogLevel::Error,
                                    "Malformed project entry (missing '='): \"" + trimmed + "\"",
                                    filePath.c_str(), fileLineNumber);
                continue;
            }

            std::string key = Trim(trimmed.substr(0, eqPos));
            std::string val = Trim(trimmed.substr(eqPos + 1));

            if (currentSection == ProjectSection::Unknown) {
                continue;
            }

            // Support prefixed keys like "input.jump" anywhere
            if (key.rfind("input.", 0) == 0) {
                upsertInputAction(key.substr(6), val);
                continue;
            }

            if (currentSection == ProjectSection::InputMap) {
                upsertInputAction(key, val);
                continue;
            }

            // Support both [Project] and [Audio] keys (including legacy/alias names)
            if (key == "name" || key == "project.name" || key == "projectName" || key == "title") {
                outConfig.name = val;
                parsedAnyEntry = true;
            } else if (key == "version" || key == "project.version") {
                outConfig.version = val;
                parsedAnyEntry = true;
            } else if (key == "windowWidth" || key == "project.windowWidth" || key == "window.width" || key == "width") {
                outConfig.windowWidth = std::atoi(val.c_str());
                parsedAnyEntry = true;
            } else if (key == "windowHeight" || key == "project.windowHeight" || key == "window.height" || key == "height") {
                outConfig.windowHeight = std::atoi(val.c_str());
                parsedAnyEntry = true;
            } else if (key == "targetFps" || key == "project.targetFps" || key == "window.fps" || key == "fps") {
                outConfig.targetFps = std::atoi(val.c_str());
                parsedAnyEntry = true;
            } else if (key == "fullscreen" || key == "project.fullscreen" || key == "window.fullscreen") {
                outConfig.fullscreen = ParseBoolValue(val);
                parsedAnyEntry = true;
            } else if (key == "resizable" || key == "project.resizable" || key == "window.resizable") {
                outConfig.resizable = ParseBoolValue(val);
                parsedAnyEntry = true;
            } else if (key == "startScene" || key == "project.startScene" || key == "initialScene" || key == "initial_scene" || key == "mainScene") {
                outConfig.startScene = val;
                parsedAnyEntry = true;
            } else if (key == "masterVolume" || key == "audio.masterVolume" || key == "master_volume" || key == "master") {
                outConfig.masterVolume = std::strtof(val.c_str(), nullptr);
                parsedAnyEntry = true;
            } else if (key == "sfxVolume" || key == "audio.sfxVolume" || key == "sfx_volume" || key == "sfx") {
                outConfig.sfxVolume = std::strtof(val.c_str(), nullptr);
                parsedAnyEntry = true;
            } else if (key == "musicVolume" || key == "audio.musicVolume" || key == "music_volume" || key == "music") {
                outConfig.musicVolume = std::strtof(val.c_str(), nullptr);
                parsedAnyEntry = true;
            } else {
                Core::Logger::Write(Core::LogLevel::Warning,
                                    "Unknown project property key '" + key + "'",
                                    filePath.c_str(), fileLineNumber);
            }
        }

        if (!parsedAnyEntry) {
            TALOS_LOG_WARN("Project file '" + filePath + "' contained no valid project entries.");
            return false;
        }

        SanitizeConfig(outConfig);

        if (outConfig.inputActions.empty()) {
            if (Core::InputMap::GetActions().empty()) {
                Core::InputMap::InitDefaults();
            }
            outConfig.inputActions = Core::InputMap::GetActions();
        }

        if (applyToSubsystems) {
            ApplyToEngine(outConfig);
        }

        TALOS_LOG_INFO("Project loaded from '" + filePath + "' (title='" + outConfig.name +
                       "', startScene='" + outConfig.startScene + "')");
        return true;
    }

    bool ProjectSerializer::CreateNewProject(ProjectConfig& inOutConfig,
                                             Scene& outInitialScene,
                                             const std::string& projectFilePath,
                                             bool populateStarterEntities) {
        if (projectFilePath.empty()) {
            TALOS_LOG_ERROR("ProjectSerializer::CreateNewProject failed: empty project file path.");
            return false;
        }

        SanitizeConfig(inOutConfig);

        Core::InputMap::InitDefaults();
        inOutConfig.inputActions = Core::InputMap::GetActions();

        Audio::AudioSystem::SetMasterVolume(inOutConfig.masterVolume);
        Audio::AudioSystem::SetSFXVolume(inOutConfig.sfxVolume);
        Audio::AudioSystem::SetMusicBusVolume(inOutConfig.musicVolume);

        if (populateStarterEntities) {
            PopulateDefaultStarterScene(outInitialScene);
        } else {
            outInitialScene.Clear();
        }

        std::string resolvedScenePath = ResolveScenePath(projectFilePath, inOutConfig.startScene);
        if (!SceneSerializer::Save(outInitialScene, resolvedScenePath)) {
            return false;
        }

        if (!Save(inOutConfig, projectFilePath)) {
            return false;
        }

        ApplyToEngine(inOutConfig);
        TALOS_LOG_INFO("Created new project '" + inOutConfig.name + "' at '" + projectFilePath +
                       "' with initial scene '" + resolvedScenePath + "'");
        return true;
    }

    bool ProjectSerializer::OpenProject(ProjectConfig& outConfig,
                                        Scene& outInitialScene,
                                        const std::string& projectFilePath) {
        if (!Load(outConfig, projectFilePath, true)) {
            return false;
        }

        std::string resolvedScenePath = ResolveScenePath(projectFilePath, outConfig.startScene);
        if (!SceneSerializer::Load(outInitialScene, resolvedScenePath)) {
            TALOS_LOG_WARN("Initial scene '" + resolvedScenePath +
                           "' not found while opening project; generating default starter scene.");
            PopulateDefaultStarterScene(outInitialScene);
            SceneSerializer::Save(outInitialScene, resolvedScenePath);
        }

        return true;
    }

    bool ProjectSerializer::SaveProject(ProjectConfig& inOutConfig,
                                        const Scene* activeScene,
                                        const std::string& projectFilePath,
                                        const std::string& sceneFilePath,
                                        bool captureEngineState) {
        if (captureEngineState) {
            CaptureFromEngine(inOutConfig);
        }
        SanitizeConfig(inOutConfig);

        if (!Save(inOutConfig, projectFilePath)) {
            return false;
        }

        if (activeScene != nullptr) {
            std::string targetScene = sceneFilePath.empty()
                ? ResolveScenePath(projectFilePath, inOutConfig.startScene)
                : sceneFilePath;
            if (!SceneSerializer::Save(*activeScene, targetScene)) {
                return false;
            }
        }

        ApplyToEngine(inOutConfig);
        return true;
    }

}
