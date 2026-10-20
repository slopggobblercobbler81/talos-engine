#include "renderer/renderer2d.h"
#include "core/logger.h"
#include "raylib.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Talos::Renderer {

    // Normalizes a resource file path (trims whitespace, converts '\' to '/', strips leading "./").
    static std::string NormalizeResourceKey(const char* rawPath) {
        if (!rawPath || rawPath[0] == '\0') {
            return "";
        }
        std::string s(rawPath);
        const auto first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return "";
        }
        const auto last = s.find_last_not_of(" \t\r\n");
        s = s.substr(first, last - first + 1);

        for (char& c : s) {
            if (c == '\\') {
                c = '/';
            }
        }
        while (s.size() >= 2 && s[0] == '.' && s[1] == '/') {
            s.erase(0, 2);
        }
        std::string collapsed;
        collapsed.reserve(s.size());
        bool prevSlash = false;
        for (char c : s) {
            if (c == '/') {
                if (!prevSlash) {
                    collapsed.push_back(c);
                }
                prevSlash = true;
            } else {
                collapsed.push_back(c);
                prevSlash = false;
            }
        }
        return collapsed;
    }

    // =========================================================================
    // Internal Typography Cache (Private to Renderer2D)
    // =========================================================================
    // Raylib's built-in default font is a blocky retro bitmap font. To provide crisp,
    // modern sans-serif text across any font size without scaling blur, Renderer2D
    // dynamically rasterizes a clean TrueType font (.ttf) at each requested pixel size
    // and caches the resulting GPU texture atlas in s_fontCache.
    static std::unordered_map<int, Font> s_fontCache;
    static std::string s_activeFontPath;

    struct CachedCustomFont {
        std::unordered_map<int, Font> sizeAtlases;
        std::unordered_set<int> requestedSizes;
        int defaultSize = 24;
        size_t fileSizeBytes = 0;
        bool uploadedToGpu = false;
        bool valid = false;
    };
    static std::unordered_map<std::string, CachedCustomFont> s_customFontCache;

    // =========================================================================
    // Internal 2D Texture Cache (Private to Renderer2D)
    // =========================================================================
    // Maps a relative or absolute file path (e.g., "assets/textures/hero.png") to its
    // GPU-resident Texture2D handle. Ensures each image file is decoded from disk and
    // uploaded to VRAM only once, even when shared across hundreds of scene entities.
    static std::unordered_map<std::string, Texture2D> s_textureCache;
    static std::unordered_set<std::string> s_cpuOnlyTextures;
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

    static bool IsValidFontExtension(const std::string& path) {
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return ext == ".ttf" || ext == ".otf";
    }

    static bool ValidateFontFileSignature(const std::string& path, size_t& outFileBytes) {
        outFileBytes = 0;
        if (!IsValidFontExtension(path)) {
            return false;
        }
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || std::filesystem::is_directory(path, ec)) {
            return false;
        }
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in.is_open()) {
            return false;
        }
        auto endPos = in.tellg();
        if (endPos < 12) {
            return false;
        }
        outFileBytes = static_cast<size_t>(endPos);
        in.seekg(0, std::ios::beg);
        unsigned char sig[4] = { 0, 0, 0, 0 };
        in.read(reinterpret_cast<char*>(sig), 4);
        if (!in.good()) {
            return false;
        }
        // TrueType (0x00010000 or 'true'), OpenType ('OTTO'), or TTC ('ttcf')
        const bool isTrueType = (sig[0] == 0x00 && sig[1] == 0x01 && sig[2] == 0x00 && sig[3] == 0x00) ||
                                (sig[0] == 't'  && sig[1] == 'r'  && sig[2] == 'u'  && sig[3] == 'e');
        const bool isOpenType = (sig[0] == 'O'  && sig[1] == 'T'  && sig[2] == 'T'  && sig[3] == 'O');
        const bool isTtc      = (sig[0] == 't'  && sig[1] == 't'  && sig[2] == 'c'  && sig[3] == 'f');
        return isTrueType || isOpenType || isTtc;
    }

    // Retrieves (or rasterizes on first request) a crisp 1:1 native Font atlas for 'fontSize'.
    static Font GetFontForSize(int fontSize) {
        int clampedSize = (fontSize < 10) ? 10 : ((fontSize > 96) ? 96 : fontSize);

        if (s_activeFontPath.empty() || !IsWindowReady()) {
            return GetFontDefault();
        }

        auto it = s_fontCache.find(clampedSize);
        if (it != s_fontCache.end()) {
            return it->second;
        }

        // Rasterize the TrueType font at the exact pixel height and apply bilinear smoothing.
        Font loadedFont = ::LoadFontEx(s_activeFontPath.c_str(), clampedSize, nullptr, 0);
        SetTextureFilter(loadedFont.texture, TEXTURE_FILTER_BILINEAR);
        s_fontCache[clampedSize] = loadedFont;
        return loadedFont;
    }

    static Font GetCustomFontForSize(const char* fontPath, int fontSize) {
        std::string key = NormalizeResourceKey(fontPath);
        if (key.empty()) {
            return GetFontForSize(fontSize);
        }
        int clampedSize = (fontSize < 10) ? 10 : ((fontSize > 96) ? 96 : fontSize);

        if (!Renderer2D::HasFont(key.c_str()) && !Renderer2D::LoadFont(key.c_str(), clampedSize)) {
            return GetFontForSize(clampedSize);
        }
        if (!IsWindowReady()) {
            return GetFontDefault();
        }
        auto it = s_customFontCache.find(key);
        if (it == s_customFontCache.end()) {
            return GetFontForSize(clampedSize);
        }
        auto sizeIt = it->second.sizeAtlases.find(clampedSize);
        if (sizeIt != it->second.sizeAtlases.end() && sizeIt->second.texture.id != 0) {
            return sizeIt->second;
        }
        Font loadedFont = ::LoadFontEx(key.c_str(), clampedSize, nullptr, 0);
        if (loadedFont.texture.id == 0) {
            return GetFontForSize(clampedSize);
        }
        SetTextureFilter(loadedFont.texture, TEXTURE_FILTER_BILINEAR);
        it->second.sizeAtlases[clampedSize] = loadedFont;
        it->second.requestedSizes.insert(clampedSize);
        it->second.uploadedToGpu = true;
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

        // 5. 4-Frame 256x64 Animated Sprite Sheet ("examples/assets/textures/sheet.png")
        // Four 64x64 animation frames at sourceX = 0, 64, 128, 192 depicting a 4-frame character walk & pulse cycle
        if (!FileExists("examples/assets/textures/sheet.png")) {
            Image img = GenImageColor(256, 64, { 0, 0, 0, 0 });

            const int bobOffsets[4]     = { 0, -2, 0, 2 };
            const int leftLegX[4]       = { 10, 18, 28, 18 };
            const int rightLegX[4]      = { 34, 28, 14, 28 };
            const int emitterWidths[4]  = { 7, 11, 8, 12 };
            const Color coreColors[4]   = {
                { 255, 210, 75, 255 },
                { 115, 250, 255, 255 },
                { 255, 225, 95, 255 },
                { 255, 150, 65, 255 }
            };

            for (int f = 0; f < 4; ++f) {
                int bx = f * 64;
                int by = bobOffsets[f];

                // Animated walking boots at bottom of frame
                ImageDrawRectangle(&img, bx + leftLegX[f],  52, 12, 10, { 26, 68, 122, 255 });
                ImageDrawRectangle(&img, bx + rightLegX[f], 52, 12, 10, { 42, 105, 180, 255 });

                // Right-side antenna / shoulder pauldron (moves with body bob)
                ImageDrawRectangle(&img, bx + 44, 2 + by, 6, 7, coreColors[f]);

                // Outer & inner armor torso
                ImageDrawRectangle(&img, bx + 6,  8 + by, 48, 44, { 35, 95, 165, 255 });
                ImageDrawRectangle(&img, bx + 10, 12 + by, 40, 36, { 65, 150, 235, 255 });

                // Dark visor band shifted right (facing right by default)
                ImageDrawRectangle(&img, bx + 15, 18 + by, 35, 15, { 20, 26, 40, 255 });

                // Glowing cyan eyes + directional pupils
                ImageDrawRectangle(&img, bx + 19, 21 + by, 9, 9, { 110, 245, 255, 255 });
                ImageDrawRectangle(&img, bx + 24, 23 + by, 4, 5, { 255, 255, 255, 255 });
                ImageDrawRectangle(&img, bx + 36, 21 + by, 9, 9, { 110, 245, 255, 255 });
                ImageDrawRectangle(&img, bx + 41, 23 + by, 4, 5, { 255, 255, 255, 255 });

                // Pulsing chest reactor core
                ImageDrawRectangle(&img, bx + 21, 37 + by, 16, 8, coreColors[f]);

                // Right-hand animated plasma emitter
                ImageDrawRectangle(&img, bx + 51, 26 + by, emitterWidths[f], 14, { 95, 245, 255, 255 });

                // Frame indicator pip bar along top-left of torso (1..4 pips)
                for (int p = 0; p <= f; ++p) {
                    ImageDrawRectangle(&img, bx + 12 + p * 6, 13 + by, 4, 3, { 255, 240, 130, 255 });
                }
            }

            ExportImage(img, "examples/assets/textures/sheet.png");
            UnloadImage(img);
        }
    }

    void Renderer2D::EnsureStarterAssetsExist() {
        EnsureStarterTexturesExist();
    }

    // Internal helper: Retrieves a cached GPU texture (or CPU metadata entry in headless mode)
    // or lazily loads it from disk on first request.
    static const Texture2D* GetOrLoadTexture(const char* texturePath) {
        std::string key = NormalizeResourceKey(texturePath);
        if (key.empty()) {
            return nullptr;
        }

        // 1. Return immediately if already resident in cache (upgrading CPU-only entry if GPU is now ready)
        auto it = s_textureCache.find(key);
        if (it != s_textureCache.end()) {
            if (IsWindowReady() && s_cpuOnlyTextures.find(key) != s_cpuOnlyTextures.end()) {
                Texture2D gpuTex = ::LoadTexture(key.c_str());
                if (gpuTex.id != 0) {
                    SetTextureFilter(gpuTex, TEXTURE_FILTER_POINT);
                    it->second = gpuTex;
                    s_cpuOnlyTextures.erase(key);
                }
            }
            return &it->second;
        }

        // 2. Skip redundant filesystem checks if already known to be missing
        if (s_missingTexturePaths.find(key) != s_missingTexturePaths.end()) {
            return nullptr;
        }

        // 3. Check disk and decode/upload if valid
        if (!FileExists(key.c_str())) {
            s_missingTexturePaths.insert(key);
            TALOS_LOG_ERROR("Texture file not found on disk: '" + key + "'");
            return nullptr;
        }

        if (IsWindowReady()) {
            Texture2D tex = ::LoadTexture(key.c_str());
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

        // Headless fallback: decode image dimensions/format in CPU RAM for unit tests and pre-window validation
        Image img = ::LoadImage(key.c_str());
        if (img.data == nullptr || img.width <= 0 || img.height <= 0) {
            if (img.data != nullptr) {
                ::UnloadImage(img);
            }
            s_missingTexturePaths.insert(key);
            TALOS_LOG_ERROR("Failed to decode texture image: '" + key + "'");
            return nullptr;
        }

        Texture2D metaTex = {};
        metaTex.id      = 1;
        metaTex.width   = img.width;
        metaTex.height  = img.height;
        metaTex.mipmaps = (img.mipmaps > 0) ? img.mipmaps : 1;
        metaTex.format  = img.format;
        ::UnloadImage(img);

        s_cpuOnlyTextures.insert(key);
        auto inserted = s_textureCache.emplace(key, metaTex);
        TALOS_LOG_INFO("Loaded 2D texture metadata (headless): '" + key + "' (" +
                       std::to_string(metaTex.width) + "x" + std::to_string(metaTex.height) + ")");
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
        // Free all GPU texture atlases allocated by the default TrueType font cache.
        for (auto& pair : s_fontCache) {
            if (IsWindowReady()) {
                ::UnloadFont(pair.second);
            }
        }
        s_fontCache.clear();

        // Free all custom project font atlases.
        UnloadAllFonts();

        // Free all cached 2D sprite textures from GPU VRAM.
        UnloadAllTextures();
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

    void Renderer2D::DrawCircle(float centerX, float centerY, float radius, ColorRGBA color) {
        if (radius <= 0.0f) {
            return;
        }
        DrawCircleV({ centerX, centerY }, radius, { color.r, color.g, color.b, color.a });
    }

    void Renderer2D::DrawCircleOutline(float centerX, float centerY, float radius, float thickness, ColorRGBA color) {
        if (radius <= 0.0f) {
            return;
        }
        float halfThick = (thickness > 1.0f) ? (thickness * 0.5f) : 0.5f;
        float innerR = (radius > halfThick) ? (radius - halfThick) : 0.0f;
        float outerR = radius + halfThick;
        DrawRing({ centerX, centerY }, innerR, outerR, 0.0f, 360.0f, 48, { color.r, color.g, color.b, color.a });
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

    bool Renderer2D::LoadTexture(const char* texturePath) {
        std::string key = NormalizeResourceKey(texturePath);
        if (key.empty()) {
            return false;
        }
        // Clear negative cache entry on explicit load so newly created files can be loaded
        s_missingTexturePaths.erase(key);
        return GetOrLoadTexture(key.c_str()) != nullptr;
    }

    bool Renderer2D::IsTextureLoaded(const char* texturePath) {
        std::string key = NormalizeResourceKey(texturePath);
        if (key.empty()) {
            return false;
        }
        return s_textureCache.find(key) != s_textureCache.end();
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

    void Renderer2D::UnloadTexture(const char* texturePath) {
        std::string key = NormalizeResourceKey(texturePath);
        if (key.empty()) {
            return;
        }
        s_missingTexturePaths.erase(key);
        auto it = s_textureCache.find(key);
        if (it == s_textureCache.end()) {
            return;
        }
        bool isCpuOnly = (s_cpuOnlyTextures.find(key) != s_cpuOnlyTextures.end());
        if (!isCpuOnly && IsWindowReady() && it->second.id != 0) {
            ::UnloadTexture(it->second);
        }
        s_cpuOnlyTextures.erase(key);
        s_textureCache.erase(it);
        TALOS_LOG_INFO("Unloaded 2D texture from cache: '" + key + "'");
    }

    void Renderer2D::UnloadAllTextures() {
        for (auto& pair : s_textureCache) {
            bool isCpuOnly = (s_cpuOnlyTextures.find(pair.first) != s_cpuOnlyTextures.end());
            if (!isCpuOnly && IsWindowReady() && pair.second.id != 0) {
                ::UnloadTexture(pair.second);
            }
        }
        s_textureCache.clear();
        s_cpuOnlyTextures.clear();
        s_missingTexturePaths.clear();
    }

    std::vector<std::string> Renderer2D::GetLoadedTexturePaths() {
        std::vector<std::string> paths;
        paths.reserve(s_textureCache.size());
        for (const auto& pair : s_textureCache) {
            paths.push_back(pair.first);
        }
        std::sort(paths.begin(), paths.end());
        return paths;
    }

    bool Renderer2D::LoadFont(const char* fontPath, int defaultFontSize) {
        std::string key = NormalizeResourceKey(fontPath);
        if (key.empty()) {
            return false;
        }

        int clampedSize = (defaultFontSize < 10) ? 10 : ((defaultFontSize > 96) ? 96 : defaultFontSize);

        auto existing = s_customFontCache.find(key);
        if (existing != s_customFontCache.end() && existing->second.valid) {
            if (IsWindowReady() && existing->second.sizeAtlases.find(clampedSize) == existing->second.sizeAtlases.end()) {
                Font f = ::LoadFontEx(key.c_str(), clampedSize, nullptr, 250);
                if (f.texture.id != 0) {
                    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
                    existing->second.sizeAtlases[clampedSize] = f;
                }
            }
            return true;
        }

        if (!IsValidFontExtension(key)) {
            TALOS_LOG_ERROR("Unsupported font extension for '" + key + "' (expected .ttf or .otf)");
            return false;
        }

        size_t fileBytes = 0;
        if (!ValidateFontFileSignature(key, fileBytes)) {
            TALOS_LOG_ERROR("Font file not found or invalid signature: '" + key + "'");
            return false;
        }

        CachedCustomFont entry = {};
        entry.valid = true;
        entry.fileSizeBytes = fileBytes;

        if (IsWindowReady()) {
            Font f = ::LoadFontEx(key.c_str(), clampedSize, nullptr, 250);
            if (f.texture.id == 0) {
                TALOS_LOG_ERROR("Failed to rasterize TrueType/OpenType font: '" + key + "'");
                return false;
            }
            SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
            entry.sizeAtlases[clampedSize] = f;
        }

        s_customFontCache[key] = std::move(entry);
        TALOS_LOG_INFO("Loaded custom font into cache: '" + key + "'");
        return true;
    }

    bool Renderer2D::HasFont(const char* fontPath) {
        std::string key = NormalizeResourceKey(fontPath);
        if (key.empty()) {
            return false;
        }
        auto it = s_customFontCache.find(key);
        return (it != s_customFontCache.end() && it->second.valid);
    }

    void Renderer2D::UnloadFont(const char* fontPath) {
        std::string key = NormalizeResourceKey(fontPath);
        if (key.empty()) {
            return;
        }
        auto it = s_customFontCache.find(key);
        if (it == s_customFontCache.end()) {
            return;
        }
        if (IsWindowReady()) {
            for (auto& atlasPair : it->second.sizeAtlases) {
                if (atlasPair.second.texture.id != 0) {
                    ::UnloadFont(atlasPair.second);
                }
            }
        }
        s_customFontCache.erase(it);
        TALOS_LOG_INFO("Unloaded custom font from cache: '" + key + "'");
    }

    void Renderer2D::UnloadAllFonts() {
        if (IsWindowReady()) {
            for (auto& fontPair : s_customFontCache) {
                for (auto& atlasPair : fontPair.second.sizeAtlases) {
                    if (atlasPair.second.texture.id != 0) {
                        ::UnloadFont(atlasPair.second);
                    }
                }
            }
        }
        s_customFontCache.clear();
    }

    std::vector<std::string> Renderer2D::GetLoadedFontPaths() {
        std::vector<std::string> paths;
        paths.reserve(s_customFontCache.size());
        for (const auto& pair : s_customFontCache) {
            if (pair.second.valid) {
                paths.push_back(pair.first);
            }
        }
        std::sort(paths.begin(), paths.end());
        return paths;
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

    void Renderer2D::DrawLabelEx(const char* fontPath, const char* text, float x, float y, int fontSize, ColorRGBA color) {
        Font font = GetCustomFontForSize(fontPath, fontSize);
        Vector2 position = { x, y };
        DrawTextEx(
            font,
            text ? text : "",
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

    int Renderer2D::MeasureTextWidthEx(const char* fontPath, const char* text, int fontSize) {
        if (!text || text[0] == '\0') {
            return 0;
        }
        if (!IsWindowReady()) {
            int clampedSize = (fontSize < 10) ? 10 : ((fontSize > 96) ? 96 : fontSize);
            return static_cast<int>(std::strlen(text)) * (clampedSize / 2);
        }
        Font font = GetCustomFontForSize(fontPath, fontSize);
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
        std::string key = NormalizeResourceKey(texturePath);
        if (key.empty()) {
            return 0;
        }
        auto it = s_textureCache.find(key);
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

    size_t Renderer2D::GetCustomFontVRAMBytes(const char* fontPath) {
        std::string key = NormalizeResourceKey(fontPath);
        if (key.empty()) {
            return 0;
        }
        auto it = s_customFontCache.find(key);
        if (it == s_customFontCache.end() || !it->second.valid) {
            return 0;
        }
        size_t total = 0;
        for (const auto& atlasPair : it->second.sizeAtlases) {
            total += ComputeTextureVRAMBytes(atlasPair.second.texture);
        }
        if (total == 0) {
            // In headless mode (or before any size atlas is uploaded), report the backing font file size
            // or a standard 256x256 RGBA atlas estimate so memory telemetry is non-zero and deterministic.
            return (it->second.fileSizeBytes > 0) ? it->second.fileSizeBytes : (256u * 256u * 4u);
        }
        return total;
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
        for (const auto& fontPair : s_customFontCache) {
            if (!fontPair.second.valid) {
                continue;
            }
            size_t fontTotal = 0;
            for (const auto& atlasPair : fontPair.second.sizeAtlases) {
                fontTotal += ComputeTextureVRAMBytes(atlasPair.second.texture);
            }
            if (fontTotal == 0 && !IsWindowReady()) {
                fontTotal = (fontPair.second.fileSizeBytes > 0) ? fontPair.second.fileSizeBytes : (256u * 256u * 4u);
            }
            total += fontTotal;
        }
        return total;
    }

    size_t Renderer2D::GetCachedTextureCount() {
        return s_textureCache.size();
    }

    size_t Renderer2D::GetCachedFontCount() {
        return s_customFontCache.size();
    }

}