#include "core/window.h"
#include "core/input.h"
#include "renderer/renderer2d.h"

int main() {
    using namespace Talos::Core;
    using namespace Talos::Renderer;

    // 1. Start the Window layer
    Window::Init(1280, 720, "Talos Engine - Layer Architecture");

    float playerX = 600.0f;
    float playerY = 320.0f;
    float speed = 300.0f;

    // 2. Engine main loop
    while (!Window::ShouldClose()) {
        float delta = Window::GetDeltaTime();

        // Update state using the Input Layer
        if (Input::IsKeyDown(Key::D) || Input::IsKeyDown(Key::Right)) playerX += speed * delta;
        if (Input::IsKeyDown(Key::A) || Input::IsKeyDown(Key::Left))  playerX -= speed * delta;
        if (Input::IsKeyDown(Key::S) || Input::IsKeyDown(Key::Down))  playerY += speed * delta;
        if (Input::IsKeyDown(Key::W) || Input::IsKeyDown(Key::Up))    playerY -= speed * delta;


        // Draw using only our Renderer2D Layer
        Renderer2D::BeginFrame({ 30, 30, 36, 255 });

            Renderer2D::DrawLabel("Talos Engine v0.1", 30, 30, 28, { 240, 240, 240, 255 });
            Renderer2D::DrawLabel("Layered architecture ready and operational", 30, 70, 18, { 150, 150, 160, 255 });
            Renderer2D::DrawRect(playerX, playerY, 80, 80, { 80, 180, 250, 255 });
        
        Renderer2D::EndFrame();
    }

    // 3. Close the Window layer
    Window::Close();
    return 0;
}