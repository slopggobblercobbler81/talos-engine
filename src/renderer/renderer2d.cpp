#include "renderer/renderer2d.h"
#include "raylib.h"

namespace Talos::Renderer {

    void Renderer2D::BeginFrame(ColorRGBA clearColor) {
        // Open the rendering pass on the active graphics context.
        BeginDrawing();

        // Clear the previous frame's color buffer to prevent visual ghosting.
        ClearBackground({ clearColor.r, clearColor.g, clearColor.b, clearColor.a });
    }

    void Renderer2D::EndFrame() {
        // Submit batched geometry to the GPU and present the rendered frame.
        EndDrawing();
    }

    void Renderer2D::DrawRect(float x, float y, float width, float height, ColorRGBA color) {
        // Convert floating-point world coordinates to integer pixel positions
        // and forward the engine ColorRGBA struct to the backend Color type.
        DrawRectangle(
            static_cast<int>(x),
            static_cast<int>(y),
            static_cast<int>(width),
            static_cast<int>(height),
            { color.r, color.g, color.b, color.a }
        );
    }

    void Renderer2D::DrawRectOutline(float x, float y, float width, float height, float thickness, ColorRGBA color) {
        // DrawRectangleLinesEx accepts floating-point rectangle bounds and border width.
        Rectangle rect = { x, y, width, height };
        DrawRectangleLinesEx(rect, thickness, { color.r, color.g, color.b, color.a });
    }

    void Renderer2D::DrawLine2D(float startX, float startY, float endX, float endY, ColorRGBA color) {
        DrawLine(
            static_cast<int>(startX),
            static_cast<int>(startY),
            static_cast<int>(endX),
            static_cast<int>(endY),
            { color.r, color.g, color.b, color.a }
        );
    }

    void Renderer2D::DrawLabel(const char* text, float x, float y, int fontSize, ColorRGBA color) {
        // Render bitmap/vector text using the default engine font at the specified pixel size.
        DrawText(
            text,
            static_cast<int>(x),
            static_cast<int>(y),
            fontSize,
            { color.r, color.g, color.b, color.a }
        );
    }

    int Renderer2D::MeasureTextWidth(const char* text, int fontSize) {
        return ::MeasureText(text, fontSize);
    }

}