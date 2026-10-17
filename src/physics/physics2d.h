#pragma once

namespace Talos::Physics {

    // Pure mathematical 2D bounding box (independent of Scene layer)
    struct Rect2D {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    // Dedicated 2D Physics and Collision subsystem (Layer 2)
    // Has zero knowledge of the Scene or Editor layers above it.
    class Physics2D {
    public:
        static bool CheckOverlap(const Rect2D& a, const Rect2D& b);
    };

}

