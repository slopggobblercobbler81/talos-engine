#pragma once
#include <cstdint>

namespace Talos::Renderer {
    
    // Our own simple color type (R, G, B, A) with no dependency external libraries
    struct ColorRGBA {
        uint8_t r, g, b, a = 255;
    };

    // The only part of the engine permitted to be painted on-screen
    class Renderer2D {
        public:
        static void BeginFrame(ColorRGBA clearColor);
        static void EndFrame();

        static void DrawRect(float x, float y, float width, float height, ColorRGBA color);
        static void DrawLabel(const char* text, float x, float y, int fontSize, ColorRGBA color);
    };
}
