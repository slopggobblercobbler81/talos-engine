// Application entry point: window setup, starter assets and main loop
#include "core/window.h"
#include "renderer/renderer2d.h"
#include "audio/audio_system.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "editor/editor.h"

int main() {
    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Audio;
    using namespace Talos::Scene;
    using namespace Talos::Editor;

    // =========================================================================
    // 1. Engine, Renderer & Audio Initialization (Core, Renderer & Audio Layers)
    // =========================================================================
    // Opens the OS display window at 1360x780, initializes the TrueType
    // typography cache, generates starter .png textures and .wav audio clips,
    // and initializes the audio hardware device.
    constexpr int WINDOW_WIDTH = 1360;
    constexpr int WINDOW_HEIGHT = 780;
    const char* DEFAULT_SCENE_PATH = "examples/scenes/main.talos";

    Window::Init(WINDOW_WIDTH, WINDOW_HEIGHT, "Talos Engine - Visual Editor & 2D Textures");
    Renderer2D::Init();
    AudioSystem::Init();

    // =========================================================================
    // 2. World Initialization: Load Saved Scene or Create Default (Scene Layer)
    // =========================================================================
    // Attempt to load "examples/scenes/main.talos" from disk. If no saved scene exists yet
    // (e.g., on first launch), populate the default starter entities and save them
    // immediately so "examples/scenes/main.talos" is created on disk.
    Scene world;

    if (!SceneSerializer::Load(world, DEFAULT_SCENE_PATH)) {
        // Title Banner Entity (Overlay Layer)
        Entity& title = world.CreateEntity("TitleBanner", 260.0f, 75.0f);
        title.sprite.enabled = false;
        title.sprite.sortingLayer = SortingLayer::Overlay;
        title.sprite.zOrder = 20;
        title.text.enabled = true;
        title.text.content = "Welcome to Talos Engine!";
        title.text.fontSize = 28;
        title.text.color = { 240, 240, 245, 255 };

        // Player Entity: Uses 4-frame animated sheet texture and is controllable with WASD/Arrows in Play Mode
        Entity& player = world.CreateEntity("Player", 360.0f, 340.0f);
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
        player.collider.shape = Talos::Physics::ColliderShape::Box;
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

        // Wooden Crate Entity: Uses 2D Crate texture and acts as a solid obstacle on Scenery layer
        Entity& crate = world.CreateEntity("WoodenCrate", 530.0f, 430.0f);
        crate.tag = "Obstacle";
        crate.transform.width = 68.0f;
        crate.transform.height = 68.0f;
        crate.sprite.enabled = true;
        crate.sprite.texturePath = "examples/assets/textures/crate.png";
        crate.sprite.color = { 255, 255, 255, 255 };
        crate.sprite.sortingLayer = SortingLayer::Scenery;
        crate.sprite.zOrder = 0;
        crate.collider.enabled = true;
        crate.collider.shape = Talos::Physics::ColliderShape::Box;
        crate.collider.isStatic = true;
        crate.text.enabled = true;
        crate.text.content = "Crate";
        crate.text.fontSize = 16;
        crate.text.offsetY = -22.0f;

        // Solid Wall Entity: Uses 2D Brick texture and blocks movement on Scenery layer
        Entity& wall = world.CreateEntity("BrickWall", 700.0f, 230.0f);
        wall.tag = "Wall";
        wall.transform.width = 110.0f;
        wall.transform.height = 280.0f;
        wall.sprite.enabled = true;
        wall.sprite.texturePath = "examples/assets/textures/brick.png";
        wall.sprite.color = { 255, 255, 255, 255 };
        wall.sprite.sortingLayer = SortingLayer::Scenery;
        wall.sprite.zOrder = 5;
        wall.collider.enabled = true;
        wall.collider.shape = Talos::Physics::ColliderShape::Box;
        wall.collider.isStatic = true;
        wall.text.enabled = true;
        wall.text.content = "Brick Wall";
        wall.text.fontSize = 18;
        wall.text.offsetY = -24.0f;

        // Gold Coin Entity: Dynamic Circle collider demonstrating mixed Circle vs Box collisions & bouncing
        Entity& coin = world.CreateEntity("GoldCoin", 540.0f, 250.0f);
        coin.tag = "Coin";
        coin.transform.width = 56.0f;
        coin.transform.height = 56.0f;
        coin.sprite.enabled = true;
        coin.sprite.texturePath = "examples/assets/textures/coin.png";
        coin.sprite.color = { 255, 255, 255, 255 };
        coin.sprite.sortingLayer = SortingLayer::Scenery;
        coin.sprite.zOrder = 10;
        coin.collider.enabled = true;
        coin.collider.shape = Talos::Physics::ColliderShape::Circle;
        coin.collider.radius = 0.0f; // Auto-fit to min(W, H) * 0.5 = 28px
        coin.collider.restitution = 0.85f;
        coin.collider.isStatic = false;
        coin.movement.enabled = true;
        coin.movement.playerControlled = false;
        coin.movement.mass = 1.0f;
        coin.text.enabled = true;
        coin.text.content = "Coin";
        coin.text.fontSize = 16;
        coin.text.offsetY = -22.0f;

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