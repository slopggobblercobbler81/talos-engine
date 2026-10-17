#include "renderer/renderer2d.h"
#include "raylib.h"

namespace Talos::Renderer {

    void Renderer2D::BeginFrame(ColorRGBA clearColor) {
        BeginDrawing();
        ClearBackground({ clearColor.r, clearColor.g, clearColor.b, clearColor.a});
    }

    void Renderer2D::EndFrame() {
        EndDrawing();
    }

    void Renderer2D::DrawRect(float x, float y, float width, float height, ColorRGBA color) {
        DrawRectangle((int)x, (int)y, (int)width, (int)height, { color.r, color.g, color.b, color.a });
    }

    void Renderer2D::DrawLabel(const char* text, float x, float y, int fontSize, ColorRGBA color) {
        DrawText(text, (int)x, (int)y, fontSize, { color.r, color.g, color.b, color.a });
    }

}