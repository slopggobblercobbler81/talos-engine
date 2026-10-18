#pragma once
#include <string>
#include "renderer/renderer2d.h"

namespace Talos::Scene {

    // =========================================================================
    // Data-Oriented Components (Layer 3: Scene & Entity)
    // =========================================================================
    // Talos Engine replaces deep object-oriented inheritance trees with flat,
    // plain-old-data (POD) structs. Each component stores the state required
    // for a single capability and can be toggled on or off directly on any Entity.

    // Defines spatial position (top-left x, y) and bounding dimensions in 2D space.
    struct TransformComponent {
        float x = 0.0f;
        float y = 0.0f;
        float width = 64.0f;
        float height = 64.0f;
    };

    // Standard 2D sorting layers rendered from back (0) to front (4).
    namespace SortingLayer {
        constexpr int Background = 0; // Distant backdrops, skies, floors
        constexpr int Scenery    = 1; // Environment props, walls, crates, obstacles
        constexpr int Characters = 2; // Players, enemies, NPCs (Default)
        constexpr int Effects    = 3; // Collectibles, particles, projectiles, VFX
        constexpr int Overlay    = 4; // World-space UI banners, floating HUD labels
        constexpr int Count      = 5;

        inline const char* GetName(int layer) {
            switch (layer) {
                case Background: return "Background";
                case Scenery:    return "Scenery";
                case Characters: return "Characters";
                case Effects:    return "Effects";
                case Overlay:    return "Overlay";
                default:         return "Custom";
            }
        }

        inline const char* GetShortName(int layer) {
            switch (layer) {
                case Background: return "Bg";
                case Scenery:    return "Scene";
                case Characters: return "Char";
                case Effects:    return "FX";
                case Overlay:    return "UI";
                default:         return "?";
            }
        }
    }

    // Controls visual shape or 2D image/texture rendering and explicit draw depth for the entity.
    // When 'texturePath' is empty, renders a solid 2D rectangle filled with 'color'.
    // When 'texturePath' points to a .png file (e.g., "assets/textures/hero.png"),
    // renders the GPU-cached texture scaled to TransformComponent and tinted by 'color'.
    struct SpriteComponent {
        bool enabled = true;
        std::string texturePath = "";
        Renderer::ColorRGBA color = { 80, 180, 250, 255 };
        int sortingLayer = SortingLayer::Characters; // Explicit depth layer (0 = Background .. 4 = Overlay)
        int zOrder = 0;                              // Fine-grained draw order within the layer (-100 .. +100)
        float sourceX = 0.0f;                        // UV sub-rectangle top-left X in pixels
        float sourceY = 0.0f;                        // UV sub-rectangle top-left Y in pixels
        float sourceW = 0.0f;                        // UV sub-rectangle width in pixels (0 = full texture width)
        float sourceH = 0.0f;                        // UV sub-rectangle height in pixels (0 = full texture height)
        bool flipX = false;                          // Horizontal mirror (left <-> right)
        bool flipY = false;                          // Vertical mirror (top <-> bottom)

        // 2D Frame-Based Sprite Sheet Animator (integrated directly on the entity's SpriteComponent)
        int frameCount = 1;                          // Total animation frames in the sprite sheet (>= 1)
        int currentFrame = 0;                        // Active 0-based frame index (0 .. frameCount - 1)
        float fps = 8.0f;                            // Playback speed in frames per second (0 .. 60)
        bool loop = true;                            // Wrap back to frame 0 after reaching frameCount - 1
        bool isPlaying = false;                      // Advance animation frames automatically over time
        float frameTimer = 0.0f;                     // Transient runtime frame accumulator in seconds

        // Synchronizes the UV sub-rectangle (sourceX, sourceY, sourceW, sourceH) with
        // currentFrame and frameCount when frameCount > 1.
        void SyncFrameUV(int texWidth = 0, int texHeight = 0) {
            if (frameCount <= 1) {
                frameCount = 1;
                currentFrame = 0;
                return;
            }
            if (currentFrame < 0) currentFrame = 0;
            if (currentFrame >= frameCount) currentFrame = frameCount - 1;

            if (sourceW <= 0.0f) {
                sourceW = (texWidth > 0) ? (static_cast<float>(texWidth) / static_cast<float>(frameCount)) : 64.0f;
            }
            if (sourceH <= 0.0f) {
                sourceH = (texHeight > 0) ? static_cast<float>(texHeight) : 64.0f;
            }

            if (texWidth > 0 && sourceW > 0.0f) {
                int frameWInt = static_cast<int>(sourceW > 1.0f ? sourceW : 1.0f);
                int cols = texWidth / frameWInt;
                if (cols < 1) cols = 1;
                if (frameCount <= cols) {
                    sourceX = static_cast<float>(currentFrame) * sourceW;
                } else {
                    int col = currentFrame % cols;
                    int row = currentFrame / cols;
                    sourceX = static_cast<float>(col) * sourceW;
                    sourceY = static_cast<float>(row) * sourceH;
                }
            } else {
                sourceX = static_cast<float>(currentFrame) * sourceW;
            }
        }
    };

    // Provides first-level text rendering directly on an entity without requiring
    // auxiliary child nodes or nested theme override menus.
    struct TextComponent {
        bool enabled = false;
        std::string content = "New Text";
        int fontSize = 24;
        Renderer::ColorRGBA color = { 240, 240, 240, 255 };
        float offsetX = 0.0f; // Relative horizontal offset from TransformComponent.x
        float offsetY = 0.0f; // Relative vertical offset from TransformComponent.y
    };

    // Enables physical 2D bounding-box collision using the entity's Transform dimensions.
    // Eliminates the need to manually create and attach separate CollisionShape child nodes.
    struct ColliderComponent {
        bool enabled = false;
        bool isSolid = true;   // When true, prevents overlapping solid entities from passing through
        bool isStatic = false; // When true, the entity acts as an immovable obstacle (e.g., walls/floors)
    };

    // Stores velocity and optional built-in directional input control for rapid prototyping.
    struct MovementComponent {
        bool enabled = false;
        bool playerControlled = true;  // Automatically binds WASD / Arrow Keys / InputMap move_* when true
        float speed = 300.0f;          // Movement speed in pixels per second
        float velocityX = 0.0f;        // Programmatic horizontal velocity (pixels/sec)
        float velocityY = 0.0f;        // Programmatic vertical velocity (pixels/sec)
    };

}
