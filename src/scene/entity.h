#pragma once
#include <cstdint>
#include <string>
#include "scene/components.h"

namespace Talos::Scene {

    using EntityID = uint32_t;

    // Flat entity composition: An entity is an ID, a unique name, and toggleable data components.
    // Zero class inheritance is used.
    struct Entity {
        EntityID id = 0;
        std::string name = "Entity";
        bool active = true;

        TransformComponent transform;
        SpriteComponent sprite;
        TextComponent text;
        ColliderComponent collider;
        MovementComponent movement;
    };

}

