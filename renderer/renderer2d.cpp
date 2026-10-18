#include "renderer/renderer2d.h"
#include "core/logger.h"
#include "raylib.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <filesystem>

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

    // =========================================================================
    // Internal 2D Texture Cache (Private to Renderer2D)
    // =========================================================================
    // Maps a relative or absolute file path (e.g., "assets/textures/hero.png") to its
    // GPU-resident Texture2D handle. Ensures each image file is decoded from disk and
    // uploaded to VRAM only once, even when shared across hundreds of scene entities.
    static std::unordered_map<std::string, Texture2D> s_textureCache;
    static std::unordered_set<std::string> s_missingTexturePaths;

    // Locates a clean, modern sans-serif TrueType font on the host system.
    static std::string ResolveCleanFontPath() {
        const char* candidatePaths[] = {
            "examples/assets/fonts/Inter-Regular.ttf",
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

    // =========================================================================
    // Built-In Starter Texture Generator
    // =========================================================================
    // Generates four real 64x64 .png files inside "examples/assets/textures/" on first launch
    // if they are not already present on disk. This guarantees developers can test
    // 2D sprite textures, alpha transparency, and tinting immediately out of the box,
    // while also serving as real editable .png files in the workspace folder.
    static void EnsureStarterTexturesExist() {
        std::error_code ec;
        std::filesystem::create_directories("examples/assets/textures", ec);

        // 1. Hero Character Sprite ("examples/assets/textures/hero.png")
        // Regenerate once if sheet.png is not yet created so hero.png has clear directional gaze/badge for flipX testing
        if (!FileExists("examples/assets/textures/hero.png") || !FileExists("examples/assets/textures/sheet.png")) {
            Image img = GenImageColor(64, 64, { 0, 0, 0, 0 }); // Transparent background
            // Right-side antenna / shoulder pauldron (makes horizontal/vertical flip unmistakably visible)
            ImageDrawRectangle(&img, 46, 0, 6, 8, { 255, 205, 65, 255 });
            // Outer armor body
            ImageDrawRectangle(&img, 4, 6, 54, 54, { 35, 95, 165, 255 });
            ImageDrawRectangle(&img, 8, 10, 46, 46, { 65, 150, 235, 255 });
            // Dark visor band shifted slightly right (facing right by default)
            ImageDrawRectangle(&img, 14, 18, 40, 18, { 22, 28, 42, 255 });
            // Glowing cyan eyes + directional pupils looking right
            ImageDrawRectangle(&img, 18, 22, 11, 10, { 110, 245, 255, 255 });
            ImageDrawRectangle(&img, 24, 24, 5, 6, { 255, 255, 255, 255 });
            ImageDrawRectangle(&img, 38, 22, 11, 10, { 110, 245, 255, 255 });
            ImageDrawRectangle(&img, 44, 24, 5, 6, { 255, 255, 255, 255 });
            // Chest core emblem + right-hand energy emitter
            ImageDrawRectangle(&img, 22, 42, 18, 10, { 255, 210, 80, 255 });
            ImageDrawRectangle(&img, 54, 28, 8, 16, { 95, 245, 255, 255 });
            ExportImage(img, "examples/assets/textures/hero.png");
            UnloadImage(img);
        }

        // 2. Wooden Storage Crate ("examples/assets/textures/crate.png")
        if (!FileExists("examples/assets/textures/crate.png")) {
            Image img = GenImageColor(64, 64, { 125, 80, 42, 255 });
            // Inner plank fill
            ImageDrawRectangle(&img, 6, 6, 52, 52, { 168, 112, 62, 255 });
            // Horizontal plank seams
            ImageDrawRectangle(&img, 6, 22, 52, 3, { 105, 65, 32, 255 });
            ImageDrawRectangle(&img, 6, 40, 52, 3, { 105, 65, 32, 255 });
            // Center vertical brace
            ImageDrawRectangle(&img, 29, 6, 6, 52, { 138, 90, 48, 255 });
            // Corner iron bolts
            ImageDrawRectangle(&img, 8, 8, 5, 5, { 70, 75, 85, 255 });
            ImageDrawRectangle(&img, 51, 8, 5, 5, { 70, 75, 85, 255 });
            ImageDrawRectangle(&img, 8, 51, 5, 5, { 70, 75, 85, 255 });
            ImageDrawRectangle(&img, 51, 51, 5, 5, { 70, 75, 85, 255 });
            ExportImage(img, "examples/assets/textures/crate.png");
            UnloadImage(img);
        }

        // 3. Stone Brick Wall ("examples/assets/textures/brick.png")
        if (!FileExists("examples/assets/textures/brick.png")) {
            Image img = GenImageColor(64, 64, { 60, 62, 72, 255 }); // Mortar background
            // Row 1 bricks
            ImageDrawRectangle(&img, 2, 2, 28, 13, { 175, 78, 72, 255 });
            ImageDrawRectangle(&img, 34, 2, 28, 13, { 160, 70, 65, 255 });
            // Row 2 bricks (staggered)
            ImageDrawRectangle(&img, 2, 18, 14, 13, { 155, 68, 62, 255 });
            ImageDrawRectangle(&img, 20, 18, 28, 13, { 182, 84, 78, 255 });
            ImageDrawRectangle(&img, 52, 18, 10, 13, { 168, 74, 68, 255 });
            // Row 3 bricks
            ImageDrawRectangle(&img, 2, 34, 28, 13, { 165, 72, 66, 255 });
            ImageDrawRectangle(&img, 34, 34, 28, 13, { 178, 80, 74, 255 });
            // Row 4 bricks (staggered)
            ImageDrawRectangle(&img, 2, 50, 14, 12, { 172, 76, 70, 255 });
            ImageDrawRectangle(&img, 20, 50, 28, 12, { 158, 68, 64, 255 });
            ImageDrawRectangle(&img, 52, 50, 10, 12, { 175, 78, 72, 255 });
            ExportImage(img, "examples/assets/textures/brick.png");
            UnloadImage(img);
        }

        // 4. Golden Coin with Alpha Transparency ("examples/assets/textures/coin.png")
        if (!FileExists("examples/assets/textures/coin.png")) {
            Image img = GenImageColor(64, 64, { 0, 0, 0, 0 }); // Fully transparent background
            ImageDrawCircle(&img, 32, 32, 27, { 235, 165, 30, 255 });
            ImageDrawCircle(&img, 32, 32, 22, { 255, 215, 65, 255 });
            // Inner vertical emblem slot
            ImageDrawRectangle(&img, 28, 18, 8, 28, { 215, 145, 20, 255 });
            ImageDrawRectangle(&img, 29, 19, 4, 24, { 255, 245, 160, 255 });
            ExportImage(img, "examples/assets/textures/coin.png");
            UnloadImage(img);
        }

        // 5. 4-Frame 256x64 Sprite Sheet ("examples/assets/textures/sheet.png")
        // Four 64x64 tiles at sourceX = 0, 64, 128, 192 for testing UV sub-rect cropping & flipping
        if (!FileExists("examples/assets/textures/sheet.png")) {
            Image img = GenImageColor(256, 64, { 0, 0, 0, 0 });

            // Frame 0 (x = 0..63): Cyber Hero Idle (Blue/Cyan facing right)
            ImageDrawRectangle(&img, 4, 6, 52, 52, { 35, 95, 165, 255 });
            ImageDrawRectangle(&img, 8, 10, 44, 44, { 65, 150, 235, 255 });
            ImageDrawRectangle(&img, 14, 18, 38, 16, { 22, 28, 42, 255 });
            ImageDrawRectangle(&img, 20, 21, 9, 10, { 110, 245, 255, 255 });
            ImageDrawRectangle(&img, 38, 21, 9, 10, { 110, 245, 255, 255 });
            ImageDrawRectangle(&img, 22, 40, 16, 10, { 255, 210, 80, 255 });
            ImageDrawRectangle(&img, 52, 24, 8, 18, { 95, 245, 255, 255 });

            // Frame 1 (x = 64..127): Cyber Hero Dash/Attack (Emerald/Gold blade extended right)
            ImageDrawRectangle(&img, 64 + 4, 8, 48, 48, { 28, 135, 85, 255 });
            ImageDrawRectangle(&img, 64 + 8, 12, 40, 40, { 58, 205, 125, 255 });
            ImageDrawRectangle(&img, 64 + 14, 18, 36, 15, { 20, 32, 28, 255 });
            ImageDrawRectangle(&img, 64 + 22, 21, 9, 9, { 255, 235, 90, 255 });
            ImageDrawRectangle(&img, 64 + 38, 21, 9, 9, { 255, 235, 90, 255 });
            ImageDrawRectangle(&img, 64 + 48, 22, 14, 10, { 255, 220, 75, 255 });

            // Frame 2 (x = 128..191): Royal Purple Power Shield / Gem
            ImageDrawCircle(&img, 128 + 32, 32, 27, { 115, 50, 185, 255 });
            ImageDrawCircle(&img, 128 + 32, 32, 21, { 175, 95, 250, 255 });
            ImageDrawRectangle(&img, 128 + 24, 16, 16, 32, { 235, 195, 255, 255 });
            ImageDrawRectangle(&img, 128 + 16, 24, 32, 16, { 235, 195, 255, 255 });

            // Frame 3 (x = 192..255): Crimson Plasma Core / Hazard Orb
            ImageDrawCircle(&img, 192 + 32, 32, 27, { 195, 45, 55, 255 });
            ImageDrawCircle(&img, 192 + 32, 32, 20, { 250, 95, 70, 255 });
            ImageDrawCircle(&img, 192 + 38, 26, 9, { 255, 230, 110, 255 });

            ExportImage(img, "examples/assets/textures/sheet.png");
            UnloadImage(img);
        }
    }

    // Internal helper: Retrieves a cached GPU texture or lazily loads it from disk on first request.
    static const Texture2D* GetOrLoadTexture(const char* texturePath) {
        if (!texturePath || texturePath[0] == '\0') {
            return nullptr;
        }

        std::string key(texturePath);

        // 1. Return immediately if already resident in VRAM
        auto it = s_textureCache.find(key);
        if (it != s_textureCache.end()) {
            return &it->second;
        }

        // 2. Skip redundant filesystem checks if already known to be missing
        if (s_missingTexturePaths.find(key) != s_missingTexturePaths.end()) {
            return nullptr;
        }

        // 3. Check disk and upload to GPU if valid
        if (!FileExists(texturePath)) {
            s_missingTexturePaths.insert(key);
            TALOS_LOG_ERROR("Texture file not found on disk: '" + key + "'");
            return nullptr;
        }

        Texture2D tex = LoadTexture(texturePath);
        if (tex.id == 0) {
            s_missingTexturePaths.insert(key);
            TALOS_LOG_ERROR("Failed to decode or upload texture to GPU: '" + key + "'");
            return nullptr;
        }

        // Use nearest-neighbor (point) filtering so crisp pixel/2D sprites never blur when scaled.
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
        auto inserted = s_textureCache.emplace(key, tex);
        TALOS_LOG_INFO("Loaded 2D texture into VRAM: '" + key + "' (" +
                       std::to_string(tex.width) + "x" + std::to_string(tex.height) + ")");
        return &inserted.first->second;
    }

    void Renderer2D::Init() {
        s_activeFontPath = ResolveCleanFontPath();

        // Pre-warm common Editor and Scene font sizes so startup interaction is seamless.
        if (!s_activeFontPath.empty()) {
            const int commonSizes[] = { 14, 15, 16, 18, 20, 24, 28 };
            for (int sz : commonSizes) {
                GetFontForSize(sz);
            }
            TALOS_LOG_INFO("Typography initialized with TrueType font: '" + s_activeFontPath + "'");
        } else {
            TALOS_LOG_WARN("No system TrueType font found; falling back to default bitmap font");
        }

        // Generate built-in starter .png textures in "examples/assets/textures/" if not present.
        EnsureStarterTexturesExist();
        TALOS_LOG_INFO("Renderer2D subsystem initialized and starter textures verified");
    }

    void Renderer2D::Shutdown() {
        // Free all GPU texture atlases allocated by the TrueType font cache.
        for (auto& pair : s_fontCache) {
            UnloadFont(pair.second);
        }
        s_fontCache.clear();

        // Free all cached 2D sprite textures from GPU VRAM.
        for (auto& pair : s_textureCache) {
            UnloadTexture(pair.second);
        }
        s_textureCache.clear();
        s_missingTexturePaths.clear();
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

    void Renderer2D::BeginCamera2D(const Camera2DView& camera) {
        Camera2D rlCam = {};
        rlCam.target   = { camera.targetX, camera.targetY };
        rlCam.offset   = { camera.offsetX, camera.offsetY };
        rlCam.rotation = 0.0f;
        rlCam.zoom     = (camera.zoom > 0.0001f) ? camera.zoom : 1.0f;
        BeginMode2D(rlCam);
    }

    void Renderer2D::EndCamera2D() {
        EndMode2D();
    }

    void Renderer2D::BeginViewportScissor(int x, int y, int width, int height) {
        BeginScissorMode(x, y, width, height);
    }

    void Renderer2D::EndViewportScissor() {
        EndScissorMode();
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

    void Renderer2D::DrawSprite(const char* texturePath,
                                float x, float y, float width, float height,
                                ColorRGBA tint,
                                float sourceX, float sourceY,
                                float sourceW, float sourceH,
                                bool flipX, bool flipY) {
        // If no texture path is assigned, fall back to a solid colored rectangle.
        if (!texturePath || texturePath[0] == '\0') {
            DrawRect(x, y, width, height, tint);
            return;
        }

        const Texture2D* tex = GetOrLoadTexture(texturePath);
        if (tex && tex->id != 0) {
            float srcW = (sourceW > 0.0f) ? sourceW : static_cast<float>(tex->width);
            float srcH = (sourceH > 0.0f) ? sourceH : static_cast<float>(tex->height);
            Rectangle sourceRect = {
                sourceX,
                sourceY,
                flipX ? -srcW : srcW,
                flipY ? -srcH : srcH
            };
            Rectangle destRect = { x, y, width, height };
            Vector2 origin = { 0.0f, 0.0f };
            DrawTexturePro(*tex, sourceRect, destRect, origin, 0.0f, { tint.r, tint.g, tint.b, tint.a });
            return;
        }

        // Visual diagnostic fallback: Draw a 2x2 magenta/dark-gray checkerboard when a texture
        // file path is non-empty but cannot be found on disk.
        float halfW = width * 0.5f;
        float halfH = height * 0.5f;
        DrawRect(x, y, halfW, halfH, { 220, 40, 180, 255 });
        DrawRect(x + halfW, y, halfW, halfH, { 35, 35, 45, 255 });
        DrawRect(x, y + halfH, halfW, halfH, { 35, 35, 45, 255 });
        DrawRect(x + halfW, y + halfH, halfW, halfH, { 220, 40, 180, 255 });
        DrawRectOutline(x, y, width, height, 1.5f, { 255, 80, 210, 255 });
    }

    bool Renderer2D::HasTexture(const char* texturePath) {
        return GetOrLoadTexture(texturePath) != nullptr;
    }

    bool Renderer2D::GetTextureSize(const char* texturePath, int& outWidth, int& outHeight) {
        const Texture2D* tex = GetOrLoadTexture(texturePath);
        if (tex && tex->id != 0) {
            outWidth  = tex->width;
            outHeight = tex->height;
            return true;
        }
        outWidth  = 0;
        outHeight = 0;
        return false;
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

    static size_t ComputeTextureVRAMBytes(const Texture2D& tex) {
        if (tex.id == 0 || tex.width <= 0 || tex.height <= 0) {
            return 0;
        }
        int bytes = GetPixelDataSize(tex.width, tex.height, tex.format);
        if (bytes > 0) {
            return static_cast<size_t>(bytes);
        }
        return static_cast<size_t>(tex.width) * static_cast<size_t>(tex.height) * 4;
    }

    size_t Renderer2D::GetTextureVRAMBytes(const char* texturePath) {
        if (!texturePath || texturePath[0] == '\0') {
            return 0;
        }
        auto it = s_textureCache.find(texturePath);
        if (it != s_textureCache.end()) {
            return ComputeTextureVRAMBytes(it->second);
        }
        return 0;
    }

    size_t Renderer2D::GetFontAtlasVRAMBytes(int fontSize) {
        int clampedSize = (fontSize < 10) ? 10 : ((fontSize > 96) ? 96 : fontSize);
        auto it = s_fontCache.find(clampedSize);
        if (it != s_fontCache.end()) {
            return ComputeTextureVRAMBytes(it->second.texture);
        }
        return 0;
    }

    size_t Renderer2D::GetTotalTextureVRAMBytes() {
        size_t total = 0;
        for (const auto& pair : s_textureCache) {
            total += ComputeTextureVRAMBytes(pair.second);
        }
        return total;
    }

    size_t Renderer2D::GetTotalFontVRAMBytes() {
        size_t total = 0;
        for (const auto& pair : s_fontCache) {
            total += ComputeTextureVRAMBytes(pair.second.texture);
        }
        return total;
    }

    size_t Renderer2D::GetCachedTextureCount() {
        return s_textureCache.size();
    }

}