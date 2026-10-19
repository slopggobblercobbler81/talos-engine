// Application entry point: window setup, starter assets and main loop
#include "core/window.h"
#include "renderer/renderer2d.h"
#include "audio/audio_system.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "scene/project_serializer.h"
#include "editor/editor.h"

int main() {
    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Audio;
    using namespace Talos::Scene;
    using namespace Talos::Editor;

    // =========================================================================
    // 1. Project Configuration & Engine Initialization (Core, Renderer, Audio)
    // =========================================================================
    // Pre-read 'examples/project.talos' (if present on disk) to configure initial
    // window dimensions, title, and target FPS before opening the OS window.
    const char* DEFAULT_PROJECT_PATH = "examples/project.talos";
    ProjectConfig projectConfig = ProjectSerializer::MakeDefaultConfig();
    bool hasExistingProjectFile = ProjectSerializer::Load(projectConfig, DEFAULT_PROJECT_PATH, false);

    std::string windowTitle = "Talos Engine - " + projectConfig.name;
    Window::Init(projectConfig.windowWidth, projectConfig.windowHeight, windowTitle.c_str());
    Window::SetTargetFrameRate(projectConfig.targetFps);
    Renderer2D::Init();
    AudioSystem::Init();

    // =========================================================================
    // 2. World & Unified Project Initialization (Scene Layer)
    // =========================================================================
    // Open 'project.talos' (or create a default project and initial scene at
    // 'examples/scenes/main.talsc' on first launch), applying audio bus volumes
    // and logical input action bindings.
    Scene world;
    if (hasExistingProjectFile) {
        ProjectSerializer::OpenProject(projectConfig, world, DEFAULT_PROJECT_PATH);
    } else {
        std::string resolvedStartScene = ProjectSerializer::ResolveScenePath(
            DEFAULT_PROJECT_PATH, projectConfig.startScene);
        if (!SceneSerializer::Load(world, resolvedStartScene)) {
            ProjectSerializer::PopulateDefaultStarterScene(world);
            SceneSerializer::Save(world, resolvedStartScene);
        }
        ProjectSerializer::SaveProject(projectConfig, nullptr, DEFAULT_PROJECT_PATH, "", true);
    }

    // =========================================================================
    // 3. Visual Editor Initialization (Editor Layer)
    // =========================================================================
    std::string initialScenePath = ProjectSerializer::ResolveScenePath(
        DEFAULT_PROJECT_PATH, projectConfig.startScene);
    Editor editor(projectConfig.windowWidth,
                  projectConfig.windowHeight,
                  initialScenePath,
                  DEFAULT_PROJECT_PATH);
    if (!world.GetEntities().empty()) {
        // Select the Player if present, otherwise select the first entity in the scene
        Entity* player = world.FindEntity("Player");
        editor.SelectEntity(player ? player->id : world.GetEntities().front().id);
    }

    // =========================================================================
    // 4. Main Engine Frame Loop
    // =========================================================================
    while (!Window::ShouldClose()) {
        float delta = Window::GetDeltaTime();

        // Update Editor tooling (or advance Scene simulation if in Play Mode)
        editor.Update(world, delta);

        // Feed active Music streaming buffers
        AudioSystem::Update();

        // Render the 2D Viewport and Editor UI panels
        Renderer2D::BeginFrame({ 30, 31, 38, 255 });
            editor.Render(world);
        Renderer2D::EndFrame();
    }

    // =========================================================================
    // 5. Engine Shutdown (Audio, Renderer & Core Layers)
    // =========================================================================
    AudioSystem::Shutdown();
    Renderer2D::Shutdown();
    Window::Close();
    return 0;
}