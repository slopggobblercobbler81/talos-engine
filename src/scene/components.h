#pragma once
#include <string>
#include "renderer/renderer2d.h"

namespace Talos::Scene {

    // Pure data: Position and size in 2D space
    struct TransformComponent {
        float x = 0.0f;
        float y = 0.0f;
        float width = 64.0f;
        float height = 64.0f;
    };

    // Pure data: Visual rectangle / sprite representation
    struct SpriteComponent {
        bool enabled = true;
        Renderer::ColorRGBA color = { 80, 180, 250, 255 };
    };

    // Pure data: First-level text representation (no nested theme overrides)
    struct TextComponent {
        bool enabled = false;
        std::string content = "New Text";
        int fontSize = 24;
        Renderer::ColorRGBA color = { 240, 240, 240, 255 };
        float offsetX = 0.0f;
        float offsetY = 0.0f;
    };

    // Pure data: 2D Axis-Aligned Bounding Box (AABB) collision
    // Automatically matches Transform size unless custom size is desired
    struct ColliderComponent {
        bool enabled = false;
        bool isSolid = true;
        bool isStatic = false;
    };

    // Pure data: Built-in 2D movement properties
    struct MovementComponent {
        bool enabled = false;
        bool playerControlled = false;
        float speed = 300.0f;
        float velocityX = 0.0f;
        float velocityY = 0.0f;
    };

}

