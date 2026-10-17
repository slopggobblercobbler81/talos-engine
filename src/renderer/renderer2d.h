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
    // Manages vector/TrueType typography caching so all Editor and Scene text
    // renders with a clean, modern sans-serif typeface instead of bitmap pixel fonts.
    class Renderer2D {
    public:
        // Initializes the 2D renderer and loads the clean TrueType typography system.
        // Must be called once after Window::Init().
        static void Init();

        // Releases all cached GPU font atlases and rendering resources.
        // Must be called before Window::Close().
        static void Shutdown();

        // Prepares the GPU backbuffer for a new frame and fills the screen
        // with the specified background color. Must be called before any Draw* calls.
        static void BeginFrame(ColorRGBA clearColor);

        // Flushes all queued draw commands and swaps the backbuffer to the screen.
        // Must be called once at the end of every frame.
        static void EndFrame();

        // Renders a solid 2D axis-aligned rectangle in screen/world coordinates.
        static void DrawRect(float x, float y, float width, float height, ColorRGBA color);

        // Renders a hollow 2D rectangle border with configurable line thickness.
        // Used for selection gizmos, collider visualization, and Editor panel borders.
        static void DrawRectOutline(float x, float y, float width, float height, float thickness, ColorRGBA color);

        // Renders a 1-pixel geometric line segment between two 2D points.
        // Used for viewport reference grids and alignment guides.
        static void DrawLine2D(float startX, float startY, float endX, float endY, ColorRGBA color);

        // Renders anti-aliased TrueType text at the given top-left coordinates and pixel height.
        static void DrawLabel(const char* text, float x, float y, int fontSize, ColorRGBA color);

        // Computes the rendered pixel width of a TrueType text string at a given font size.
        // Used for centering labels inside buttons and positioning text cursors.
        static int MeasureTextWidth(const char* text, int fontSize);
    };

}
