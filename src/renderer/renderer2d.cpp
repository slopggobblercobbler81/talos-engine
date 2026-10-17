#include "renderer/renderer2d.h"
#include "raylib.h"
#include <unordered_map>
#include <string>

namespace Talos::Renderer {

    // =========================================================================
    // Internal Typography Cache (Private to Renderer2D)
    // =========================================================================
    // Raylib's built-in default font is a blocky retro bitmap font. To provide crisp,
    // modern sans-serif text across any font size without scaling blur, Renderer2D
    // dynamically rasterizes a clean TrueType font (.ttf) at each requested pixel size
    // and caches the resulting GPU texture atlas in s_fontCache.
    static std::unordered_map<int, Font> s_fontCache;
    static std::string s_activeFontPath;

    // Locates a clean, modern sans-serif TrueType font on the host system.
    static std::string ResolveCleanFontPath() {
        const char* candidatePaths[] = {
            "assets/fonts/Inter-Regular.ttf",
            "C:/Windows/Fonts/segoeui.ttf",   // Windows standard modern UI font (Segoe UI)
            "C:/Windows/Fonts/arial.ttf",     // Windows fallback sans-serif
            "C:/Windows/Fonts/calibri.ttf",   // Windows fallback
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", // Linux fallback
            "/System/Library/Fonts/SFNS.ttf"                   // macOS fallback
        };

        for (const char* path : candidatePaths) {
            if (FileExists(path)) {
                return path;
            }
        }
        return "";
    }

    // Retrieves (or rasterizes on first request) a crisp 1:1 native Font atlas for 'fontSize'.
    static Font GetFontForSize(int fontSize) {
        int clampedSize = (fontSize < 10) ? 10 : ((fontSize > 96) ? 96 : fontSize);

        if (s_activeFontPath.empty()) {
            return GetFontDefault();
        }

        auto it = s_fontCache.find(clampedSize);
        if (it != s_fontCache.end()) {
            return it->second;
        }

        // Rasterize the TrueType font at the exact pixel height and apply bilinear smoothing.
        Font loadedFont = LoadFontEx(s_activeFontPath.c_str(), clampedSize, nullptr, 0);
        SetTextureFilter(loadedFont.texture, TEXTURE_FILTER_BILINEAR);
        s_fontCache[clampedSize] = loadedFont;
        return loadedFont;
    }

    void Renderer2D::Init() {
        s_activeFontPath = ResolveCleanFontPath();

        // Pre-warm common Editor and Scene font sizes so startup interaction is seamless.
        if (!s_activeFontPath.empty()) {
            const int commonSizes[] = { 14, 15, 16, 18, 20, 24, 28 };
            for (int sz : commonSizes) {
                GetFontForSize(sz);
            }
        }
    }

    void Renderer2D::Shutdown() {
        // Free all GPU texture atlases allocated by the TrueType font cache.
        for (auto& pair : s_fontCache) {
            UnloadFont(pair.second);
        }
        s_fontCache.clear();
    }

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
        DrawRectangle(
            static_cast<int>(x),
            static_cast<int>(y),
            static_cast<int>(width),
            static_cast<int>(height),
            { color.r, color.g, color.b, color.a }
        );
    }

    void Renderer2D::DrawRectOutline(float x, float y, float width, float height, float thickness, ColorRGBA color) {
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
        Font font = GetFontForSize(fontSize);
        Vector2 position = { x, y };

        // DrawTextEx renders using our clean TrueType font with 0.5px letter spacing
        // instead of the blocky arcade default font.
        DrawTextEx(
            font,
            text,
            position,
            static_cast<float>(fontSize),
            0.5f,
            { color.r, color.g, color.b, color.a }
        );
    }

    int Renderer2D::MeasureTextWidth(const char* text, int fontSize) {
        Font font = GetFontForSize(fontSize);
        Vector2 measured = MeasureTextEx(font, text, static_cast<float>(fontSize), 0.5f);
        return static_cast<int>(measured.x);
    }

}