#pragma once
#include <cstdint>

namespace Talos::Renderer {

    // =========================================================================
    // Engine-Native Color Primitive
    // =========================================================================
    // Represents an 8-bit per channel RGBA color (0-255 per component).
    // Defined in the Renderer namespace so Scene and Editor layers can store
    // color values without depending on external graphics headers.
    struct ColorRGBA {
        uint8_t r = 255;
        uint8_t g = 255;
        uint8_t b = 255;
        uint8_t a = 255; // Alpha channel: 255 = fully opaque, 0 = fully transparent
    };

    // =========================================================================
    // 2D Rendering Subsystem (Layer 2: Domain Subsystems)
    // =========================================================================
    // The exclusive gateway for issuing 2D draw commands to the GPU.
    // No other module in Talos Engine is permitted to invoke raw graphics APIs.
    class Renderer2D {
    public:
        // Prepares the GPU backbuffer for a new frame and fills the screen
        // with the specified background color. Must be called before any Draw* calls.
        static void BeginFrame(ColorRGBA clearColor);

        // Flushes all queued draw commands and swaps the backbuffer to the screen.
        // Must be called once at the end of every frame.
        static void EndFrame();

        // Renders a solid 2D axis-aligned rectangle in screen/world coordinates.
        static void DrawRect(float x, float y, float width, float height, ColorRGBA color);

        // Renders a 2D text string at the given top-left coordinates and pixel height.
        static void DrawLabel(const char* text, float x, float y, int fontSize, ColorRGBA color);
    };

}
