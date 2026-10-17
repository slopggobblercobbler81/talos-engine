#include "core/window.h"
#include "renderer/renderer2d.h"
#include "scene/scene.h"

int main() {
    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Scene;

    // 1. Initialize the Window layer
    Window::Init(1280, 720, "Talos Engine - Scene & Physics Layers");

    // 2. Create our Scene and populate it with flat, zero-inheritance Entities
    Scene world;

    // Title UI Entity (Text component right at the first level, no nested containers)
    Entity& title = world.CreateEntity("TitleText", 30.0f, 25.0f);
    title.sprite.enabled = false;
    title.text.enabled = true;
    title.text.content = "Talos Engine v0.1 - Zero-Inheritance Scene & Collision";
    title.text.fontSize = 26;
    title.text.color = { 240, 240, 240, 255 };

    // Subtitle Entity
    Entity& subtitle = world.CreateEntity("SubtitleText", 30.0f, 60.0f);
    subtitle.sprite.enabled = false;
    subtitle.text.enabled = true;
    subtitle.text.content = "Move the Player (WASD/Arrows) and test solid collision against the Wall";
    subtitle.text.fontSize = 18;
    subtitle.text.color = { 150, 150, 165, 255 };

    // Player Entity (Sprite + Collider + Movement + Label all in one flat object!)
    Entity& player = world.CreateEntity("Player", 250.0f, 320.0f);
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

    // Solid Wall Obstacle Entity
    Entity& wall = world.CreateEntity("Wall", 600.0f, 220.0f);
    wall.transform.width = 120.0f;
    wall.transform.height = 280.0f;
    wall.sprite.color = { 235, 95, 95, 255 };
    wall.collider.enabled = true;
    wall.collider.isStatic = true;
    wall.text.enabled = true;
    wall.text.content = "Solid Wall";
    wall.text.fontSize = 18;
    wall.text.offsetY = -24.0f;

    // 3. Engine main loop
    while (!Window::ShouldClose()) {
        float delta = Window::GetDeltaTime();

        // Update Scene (handles movement and Physics2D collisions)
        world.Update(delta);

        // Render Scene through Renderer2D
        Renderer2D::BeginFrame({ 30, 30, 36, 255 });
            world.Render();
        Renderer2D::EndFrame();
    }

    // 4. Close the Window layer
    Window::Close();
    return 0;
}