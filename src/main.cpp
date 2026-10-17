#include "core/window.h"
#include "renderer/renderer2d.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "editor/editor.h"

int main() {
    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Scene;
    using namespace Talos::Editor;

    // =========================================================================
    // 1. Engine & Renderer Initialization (Core & Renderer Layers)
    // =========================================================================
    // Opens the OS display window at 1360x780 and initializes the TrueType
    // typography cache inside Renderer2D.
    constexpr int WINDOW_WIDTH = 1360;
    constexpr int WINDOW_HEIGHT = 780;
    const char* DEFAULT_SCENE_PATH = "scenes/main.talos";

    Window::Init(WINDOW_WIDTH, WINDOW_HEIGHT, "Talos Engine - Visual Editor & Scene Serialization");
    Renderer2D::Init();

    // =========================================================================
    // 2. World Initialization: Load Saved Scene or Create Default (Scene Layer)
    // =========================================================================
    // Attempt to load "scenes/main.talos" from disk. If no saved scene exists yet
    // (e.g., on first launch), populate the default starter entities and save them
    // immediately so "scenes/main.talos" is created on disk.
    Scene world;

    if (!SceneSerializer::Load(world, DEFAULT_SCENE_PATH)) {
        // Title Banner Entity
        Entity& title = world.CreateEntity("TitleBanner", 260.0f, 75.0f);
        title.sprite.enabled = false;
        title.text.enabled = true;
        title.text.content = "Welcome to Talos Engine!";
        title.text.fontSize = 28;
        title.text.color = { 240, 240, 245, 255 };

        // Player Entity: Controllable with WASD/Arrows when PLAY GAME is active
        Entity& player = world.CreateEntity("Player", 380.0f, 340.0f);
        player.transform.width = 70.0f;
        player.transform.height = 70.0f;
        player.sprite.color = { 80, 180, 250, 255 };
        player.collider.enabled = true;
        player.movement.enabled = true;
        player.movement.playerControlled = true;
        player.movement.speed = 320.0f;
        player.text.enabled = true;
        player.text.content = "Player";
        player.text.fontSize = 18;
        player.text.offsetY = -24.0f;

        // Solid Wall Entity: Blocks the Player during Play Mode
        Entity& wall = world.CreateEntity("SolidWall", 680.0f, 240.0f);
        wall.transform.width = 110.0f;
        wall.transform.height = 280.0f;
        wall.sprite.color = { 235, 95, 95, 255 };
        wall.collider.enabled = true;
        wall.collider.isStatic = true;
        wall.text.enabled = true;
        wall.text.content = "Wall";
        wall.text.fontSize = 18;
        wall.text.offsetY = -24.0f;

        // Persist the initial scene to disk
        SceneSerializer::Save(world, DEFAULT_SCENE_PATH);
    }

    // =========================================================================
    // 3. Visual Editor Initialization (Editor Layer)
    // =========================================================================
    Editor editor(WINDOW_WIDTH, WINDOW_HEIGHT, DEFAULT_SCENE_PATH);
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

        // Render the 2D Viewport and Editor UI panels
        Renderer2D::BeginFrame({ 30, 31, 38, 255 });
            editor.Render(world);
        Renderer2D::EndFrame();
    }

    // =========================================================================
    // 5. Engine Shutdown (Renderer & Core Layers)
    // =========================================================================
    Renderer2D::Shutdown();
    Window::Close();
    return 0;
}