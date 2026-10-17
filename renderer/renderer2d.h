#pragma once
#include <cstdint>
#include <cstddef>

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
    // Engine-Native 2D Camera View Primitive
    // =========================================================================
    // Defines a 2D orthographic view transformation (Pan & Zoom) for rendering
    // world-space scenes and converting between screen pixels and world coordinates.
    struct Camera2DView {
        float targetX = 0.0f; // World-space X coordinate anchored at 'offset'
        float targetY = 0.0f; // World-space Y coordinate anchored at 'offset'
        float offsetX = 0.0f; // Screen-space X pixel coordinate of the camera anchor
        float offsetY = 0.0f; // Screen-space Y pixel coordinate of the camera anchor
        float zoom    = 1.0f; // Magnification scale (1.0f = 100%, 2.0f = 200%, 0.5f = 50%)

        // Converts a screen-space pixel coordinate (e.g., mouse cursor) into 2D world coordinates.
        void ScreenToWorld(float screenX, float screenY, float& outWorldX, float& outWorldY) const {
            float safeZoom = (zoom > 0.0001f) ? zoom : 1.0f;
            outWorldX = (screenX - offsetX) / safeZoom + targetX;
            outWorldY = (screenY - offsetY) / safeZoom + targetY;
        }

        // Converts a 2D world coordinate into screen-space pixel coordinates.
        void WorldToScreen(float worldX, float worldY, float& outScreenX, float& outScreenY) const {
            outScreenX = (worldX - targetX) * zoom + offsetX;
            outScreenY = (worldY - targetY) * zoom + offsetY;
        }
    };

    // =========================================================================
    // 2D Rendering Subsystem (Layer 2: Domain Subsystems)
    // =========================================================================
    // The exclusive gateway for issuing 2D draw commands to the GPU.
    // Manages two internal GPU resource caches behind a clean C++ interface:
    //   1. TrueType Typography Cache: Dynamically rasterizes clean sans-serif fonts.
    //   2. 2D Texture Cache: Lazily loads and caches .png textures in VRAM by file path.
    class Renderer2D {
    public:
        // Initializes the 2D renderer, loads the clean TrueType typography system,
        // and generates starter .png texture assets in "assets/textures/" if absent.
        // Must be called once after Window::Init().
        static void Init();

        // Releases all cached GPU font atlases and 2D textures from VRAM.
        // Must be called before Window::Close().
        static void Shutdown();

        // Prepares the GPU backbuffer for a new frame and fills the screen
        // with the specified background color. Must be called before any Draw* calls.
        static void BeginFrame(ColorRGBA clearColor);

        // Flushes all queued draw commands and swaps the backbuffer to the screen.
        // Must be called once at the end of every frame.
        static void EndFrame();

        // Activates a 2D camera transformation matrix (Pan & Zoom) for all subsequent
        // world-space Draw* calls until EndCamera2D() is invoked.
        static void BeginCamera2D(const Camera2DView& camera);

        // Restores the default 1:1 screen-space coordinate matrix (used for UI overlays).
        static void EndCamera2D();

        // Restricts GPU rendering to a rectangular pixel region on screen (Scissor Clipping).
        // Prevents zoomed/panned world entities from bleeding outside the central viewport.
        static void BeginViewportScissor(int x, int y, int width, int height);

        // Disables rectangular scissor clipping so UI panels can draw across the full window.
        static void EndViewportScissor();

        // Renders a solid 2D axis-aligned rectangle in screen/world coordinates.
        static void DrawRect(float x, float y, float width, float height, ColorRGBA color);

        // Renders a hollow 2D rectangle border with configurable line thickness.
        // Used for selection gizmos, collider visualization, and Editor panel borders.
        static void DrawRectOutline(float x, float y, float width, float height, float thickness, ColorRGBA color);

        // Renders a 1-pixel geometric line segment between two 2D points.
        // Used for viewport reference grids and alignment guides.
        static void DrawLine2D(float startX, float startY, float endX, float endY, ColorRGBA color);

        // Renders a 2D image/texture scaled to the target rectangle (x, y, width, height)
        // and multiplied by 'tint' (use {255, 255, 255, 255} for unmodified original colors).
        // The texture is automatically loaded from disk on first use and cached in VRAM.
        // If the file path does not exist, renders a visual checkerboard fallback pattern.
        static void DrawSprite(const char* texturePath, float x, float y, float width, float height, ColorRGBA tint);

        // Returns true if 'texturePath' points to a valid texture that is either already
        // cached in VRAM or exists on disk and can be loaded into the texture cache.
        static bool HasTexture(const char* texturePath);

        // Renders anti-aliased TrueType text at the given top-left coordinates and pixel height.
        static void DrawLabel(const char* text, float x, float y, int fontSize, ColorRGBA color);

        // Computes the rendered pixel width of a TrueType text string at a given font size.
        // Used for centering labels inside buttons and positioning text cursors.
        static int MeasureTextWidth(const char* text, int fontSize);

        // =====================================================================
        // Diagnostic & Memory Profiling Queries (VRAM)
        // =====================================================================
        // Returns the exact GPU VRAM footprint in bytes of a cached 2D texture (0 if not loaded).
        static size_t GetTextureVRAMBytes(const char* texturePath);

        // Returns the exact GPU VRAM footprint in bytes of the TrueType font atlas for 'fontSize'.
        static size_t GetFontAtlasVRAMBytes(int fontSize);

        // Returns the total GPU VRAM bytes occupied by all cached 2D sprite textures.
        static size_t GetTotalTextureVRAMBytes();

        // Returns the total GPU VRAM bytes occupied by all cached TrueType font atlases.
        static size_t GetTotalFontVRAMBytes();

        // Returns the number of unique 2D textures currently resident in GPU VRAM.
        static size_t GetCachedTextureCount();
    };

}
