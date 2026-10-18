// Flat Entity struct aggregating all components
#pragma once
#include <cstdint>
#include <string>
#include "scene/components.h"

namespace Talos::Scene {

    // Unique 32-bit numeric identifier assigned to every entity upon creation.
    using EntityID = uint32_t;

    // =========================================================================
    // Flat Entity Composition (Layer 3: Scene & Entity)
    // =========================================================================
    // Instead of using C++ class inheritance (e.g., Player : PhysicsBody : Node2D),
    // every Entity is a uniform, contiguous data container identified by a unique ID
    // and human-readable name. Behavior is determined strictly by which components
    // have their 'enabled' flag active.
    struct Entity {
        EntityID id = 0;
        std::string name = "Entity";
        bool active = true; // Master toggle: inactive entities skip both Update and Render passes

        TransformComponent transform;
        SpriteComponent sprite;
        TextComponent text;
        ColliderComponent collider;
        MovementComponent movement;
        CameraComponent camera;
    };

}

