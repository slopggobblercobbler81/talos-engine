#include "core/window.h"
#include "renderer/renderer2d.h"

int main() {
    using namespace Talos::Core;
    using namespace Talos::Renderer;

    // 1. Start the Window layer
    Window::Init(1280, 720, "Talos Engine - Layer Architecture");

    // 2. Engine main loop
    while (!Window::ShouldClose()) {
        // Draw using only our Renderer2D Layer
        Renderer2D::BeginFrame({ 30, 30, 36, 255 });

            Renderer2D::DrawLabel("Talos Engine v0.1", 30, 30, 28, { 240, 240, 240, 255 });
            Renderer2D::DrawLabel("Layered architecture ready and operational", 30, 70, 18, { 150, 150, 160, 255 });
            Renderer2D::DrawRect(600, 320, 80, 80, { 80, 180, 250, 255 });
        
        Renderer2D::EndFrame();
    }

    // 3. Close the Window layer
    Window::Close();
    return 0;
}