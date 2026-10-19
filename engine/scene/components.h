// Flat component data structs (Transform, Sprite, Text, Collider, Movement, Camera)
#pragma once
#include <string>
#include "physics/physics2d.h"
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
        bool enabled = false;
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

    // Enables physical 2D collision (Box AABB or radial Circle) or non-blocking sensor trigger zones
    // using the entity's Transform. Eliminates the need to manually attach separate CollisionShape child nodes.
    struct ColliderComponent {
        bool enabled = false;
        bool isSolid = true;                                        // When true, prevents overlapping solid entities from passing through
        bool isStatic = false;                                      // When true, acts as an immovable obstacle (infinite mass)
        bool isTrigger = false;                                     // When true, acts as a pass-through sensor that emits overlap events without blocking movement
        Physics::ColliderShape shape = Physics::ColliderShape::Box; // Geometric primitive: Box (AABB) or Circle
        float radius = 0.0f;                                        // Custom circle radius in pixels (0.0 = auto-fit min(width, height) * 0.5)
        float restitution = 0.0f;                                   // Bounciness coefficient (0.00 = inelastic .. 1.00 = elastic)

        // Resolves the effective world-space circle radius for this entity given its Transform dimensions.
        float GetEffectiveRadius(float transformWidth, float transformHeight) const {
            if (radius > 0.5f) {
                return radius;
            }
            float minDim = (transformWidth < transformHeight) ? transformWidth : transformHeight;
            return (minDim > 1.0f) ? (minDim * 0.5f) : 1.0f;
        }

        // Constructs a Physics::Shape2D in world space at (worldX, worldY) with (width, height).
        Physics::Shape2D BuildShape2D(float worldX, float worldY, float width, float height) const {
            if (shape == Physics::ColliderShape::Circle) {
                float r = GetEffectiveRadius(width, height);
                return Physics::Shape2D::MakeCircle(worldX + width * 0.5f, worldY + height * 0.5f, r);
            }
            return Physics::Shape2D::MakeBox(worldX, worldY, width, height);
        }
    };

    // Stores velocity, physical mass/gravity, and optional built-in directional input control.
    struct MovementComponent {
        bool enabled = false;
        bool playerControlled = true;  // Automatically binds WASD / Arrow Keys / InputMap move_* when true
        float speed = 300.0f;          // Player movement speed in pixels per second
        float velocityX = 0.0f;        // Horizontal linear velocity (pixels/sec)
        float velocityY = 0.0f;        // Vertical linear velocity (pixels/sec)
        float mass = 1.0f;             // Physical mass for impulse collision resolution (>= 0.1)
        float gravityScale = 0.0f;     // Gravity multiplier (0.0 = top-down / weightless, 1.0 = standard 2D gravity)
    };

    // Configures a 2D runtime game camera for Play Mode and exported standalone games.
    // Supports either a stationary Fixed view or a Target-Following view (instant or smooth damping).
    struct CameraComponent {
        bool enabled = false;
        bool isPrimary = true;          // Prioritized when multiple active cameras exist in the scene
        bool followTarget = true;       // false = Fixed camera, true = Follow target entity
        bool smoothFollow = true;       // Exponential damping when following a target
        std::string targetName = "";    // Entity name to follow ("" or "Self" = follow owning entity)
        float followSpeed = 8.0f;       // Smooth damping speed (1.0 .. 25.0)
        float zoom = 1.0f;              // Runtime viewport zoom factor (0.25 .. 4.00)
        float offsetX = 0.0f;           // Horizontal framing offset in world pixels
        float offsetY = 0.0f;           // Vertical framing offset in world pixels

        // Transient runtime tracking state (not serialized to .talos files)
        float currentX = 0.0f;
        float currentY = 0.0f;
        bool runtimeInitialized = false;
    };

    // Configures an audio emitter (in-memory Sound effect or streaming Music track) attached to an entity.
    struct AudioComponent {
        bool enabled = false;
        std::string audioPath = "";     // Path to audio file (e.g., "examples/assets/audio/coin.wav")
        bool isMusic = false;           // false = Sound Effect (WAV/OGG in-memory), true = Music Stream (OGG/MP3/WAV)
        bool playOnStart = true;        // Automatically start playback when Play Mode begins
        bool loop = false;              // Continuously repeat playback while active in Play Mode
        bool playOnCollision = false;   // Trigger playback when this entity enters a solid collision or trigger overlap
        float volume = 1.0f;            // Playback volume multiplier (0.00 .. 1.00)
        float pitch = 1.0f;             // Playback pitch multiplier (0.25 .. 2.50, 1.00 = normal)
        float pan = 0.5f;               // Stereo pan balance (0.00 = right, 0.50 = center, 1.00 = left)

        // Transient runtime state (not serialized to .talos files)
        bool runtimeStarted = false;
    };

    using AudioSourceComponent = AudioComponent;

}

