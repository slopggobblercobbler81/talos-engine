#pragma once

namespace Talos::Physics {

    // =========================================================================
    // Pure Mathematical 2D Bounding Box (AABB)
    // =========================================================================
    // Represents an unrotated 2D rectangle defined by its top-left corner (x, y)
    // and dimensions (width, height). Kept decoupled from Scene::Entity so that
    // the Physics layer (Layer 2) never depends on the Scene layer (Layer 3).
    struct Rect2D {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    // =========================================================================
    // 2D Physics & Collision Subsystem (Layer 2: Domain Subsystems)
    // =========================================================================
    // Provides stateless geometric intersection tests for 2D collision detection.
    class Physics2D {
    public:
        // Evaluates whether two axis-aligned bounding boxes (AABBs) overlap in 2D space.
        // Returns true if and only if projections on both the X and Y axes intersect.
        static bool CheckOverlap(const Rect2D& a, const Rect2D& b);
    };

}
