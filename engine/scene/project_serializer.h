// Unified project configuration (ProjectConfig) and project.talos workflow (ProjectSerializer)
#pragma once
#include <string>
#include <vector>
#include "core/input_map.h"
#include "scene/scene.h"

namespace Talos::Scene {

    // =========================================================================
    // Project Configuration Data Structure (Layer 3: Scene & Project)
    // =========================================================================
    // Aggregates the unified project configuration persisted in 'project.talos':
    //   - [Project]  : Game title, version, window resolution, target FPS, flags, and initial scene.
    //   - [Audio]    : Master, SFX, and Music bus volume levels (0.0f to 1.0f).
    //   - [InputMap] : Logical gameplay action names mapped to physical keyboard keys.
    struct ProjectConfig {
        // General Project Configuration ([Project])
        std::string name       = "Talos Starter Project";
        std::string version    = "0.1.0";
        int windowWidth        = 1360;
        int windowHeight       = 780;
        int targetFps          = 60;
        bool fullscreen        = false;
        bool resizable         = true;
        std::string startScene = "examples/scenes/main.talsc";

        // Audio Bus Configuration ([Audio])
        float masterVolume     = 1.0f;
        float sfxVolume        = 1.0f;
        float musicVolume      = 1.0f;

        // Logical Input Map Bindings ([InputMap])
        std::vector<Core::InputAction> inputActions;
    };

    // =========================================================================
    // Unified Project Serialization & Workflow Subsystem (Layer 3: Scene)
    // =========================================================================
    // Handles creating, opening, and saving unified '.talos' project files
    // ('project.talos'), synchronizing general project settings, initial scene,
    // audio bus volumes, and configurable input action bindings.
    class ProjectSerializer {
    public:
        // Returns a clean default ProjectConfig populated with built-in InputMap actions.
        static ProjectConfig MakeDefaultConfig();

        // Clamps numeric fields and populates fallback defaults for empty strings.
        static void SanitizeConfig(ProjectConfig& inOutConfig);

        // Resolves 'scenePath' relative to 'projectFilePath' when needed.
        static std::string ResolveScenePath(const std::string& projectFilePath,
                                            const std::string& scenePath);

        // Reads live AudioSystem bus volumes and InputMap actions into 'inOutConfig'.
        static void CaptureFromEngine(ProjectConfig& inOutConfig);

        // Applies 'config' (target FPS, audio bus volumes, and InputMap actions) to engine subsystems.
        static void ApplyToEngine(const ProjectConfig& config);

        // Populates 'outScene' with the default starter entities (TitleBanner, Player, Crate, Wall, Coin).
        static void PopulateDefaultStarterScene(Scene& outScene);

        // Populates 'outScene' with a secondary starter scene ('arena.talsc') for multi-scene switching.
        static void PopulateDefaultArenaScene(Scene& outScene);

        // Writes a unified 'project.talos' file ([Project], [Audio], [InputMap]) to 'filePath'.
        static bool Save(const ProjectConfig& config,
                         const std::string& filePath = "examples/project.talos");

        // Parses a unified 'project.talos' file into 'outConfig' and optionally applies it to subsystems.
        static bool Load(ProjectConfig& outConfig,
                         const std::string& filePath = "examples/project.talos",
                         bool applyToSubsystems = true);

        // Creates a new project: resets subsystems, generates and saves the initial scene,
        // and writes the unified 'project.talos' file.
        static bool CreateNewProject(ProjectConfig& inOutConfig,
                                     Scene& outInitialScene,
                                     const std::string& projectFilePath = "examples/project.talos",
                                     bool populateStarterEntities = true);

        // Opens an existing 'project.talos', applies its settings to engine subsystems,
        // and loads its configured 'startScene' into 'outInitialScene'.
        static bool OpenProject(ProjectConfig& outConfig,
                                Scene& outInitialScene,
                                const std::string& projectFilePath = "examples/project.talos");

        // Saves the unified 'project.talos' (capturing live Audio & InputMap state if requested)
        // and optionally saves 'activeScene' to 'sceneFilePath' (or 'inOutConfig.startScene').
        static bool SaveProject(ProjectConfig& inOutConfig,
                                const Scene* activeScene = nullptr,
                                const std::string& projectFilePath = "examples/project.talos",
                                const std::string& sceneFilePath = "",
                                bool captureEngineState = true);
    };

}
