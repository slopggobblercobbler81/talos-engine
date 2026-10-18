#pragma once

namespace Talos::Physics {

    // =========================================================================
    // 2D Collider Geometric Shape Types
    // =========================================================================
    enum class ColliderShape : int {
        Box    = 0, // Axis-Aligned Bounding Box (AABB)
        Circle = 1  // Radial 2D Circle
    };

    inline const char* GetColliderShapeName(ColliderShape shape) {
        switch (shape) {
            case ColliderShape::Box:    return "Box";
            case ColliderShape::Circle: return "Circle";
            default:                    return "Box";
        }
    }

    // =========================================================================
    // Pure Mathematical 2D Primitives (Decoupled from Scene::Entity)
    // =========================================================================
    // Represents an unrotated 2D rectangle defined by its top-left corner (x, y)
    // and dimensions (width, height).
    struct Rect2D {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    // Represents a 2D circle defined by its world-space center (centerX, centerY)
    // and radial extent (radius).
    struct Circle2D {
        float centerX = 0.0f;
        float centerY = 0.0f;
        float radius = 0.0f;
    };

    // Unified 2D collision shape descriptor capable of representing either an AABB
    // Box or a Circle for polymorphic-free dispatch in Physics2D.
    struct Shape2D {
        ColliderShape type = ColliderShape::Box;
        Rect2D box = {};
        Circle2D circle = {};

        static Shape2D MakeBox(float x, float y, float width, float height) {
            Shape2D s;
            s.type = ColliderShape::Box;
            s.box = { x, y, width, height };
            float minDim = (width < height) ? width : height;
            s.circle = { x + width * 0.5f, y + height * 0.5f, minDim * 0.5f };
            return s;
        }

        static Shape2D MakeCircle(float centerX, float centerY, float radius) {
            Shape2D s;
            s.type = ColliderShape::Circle;
            s.circle = { centerX, centerY, radius };
            s.box = { centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f };
            return s;
        }
    };

    // =========================================================================
    // 2D Contact Manifold & Physical Body State
    // =========================================================================
    // Stores the geometric result of a narrow-phase intersection test between
    // shape A and shape B.
    // Convention: (normalX, normalY) is a unit vector pointing from A toward B.
    // To separate colliding shapes, move A by (-normal * penetration) and/or
    // move B by (+normal * penetration).
    struct CollisionManifold {
        bool colliding    = false; // True if shape A and shape B intersect
        float normalX     = 0.0f;  // Unit collision normal X (pointing from A to B)
        float normalY     = 0.0f;  // Unit collision normal Y (pointing from A to B)
        float penetration = 0.0f;  // Overlap depth along the collision normal (>= 0)
        float contactX    = 0.0f;  // World-space contact point X
        float contactY    = 0.0f;  // World-space contact point Y
    };

    // Lightweight kinematic & dynamic state passed into Physics2D::ResolveCollision
    // to perform Minimum Translation Vector (MTV) positional correction and
    // linear impulse velocity resolution without coupling Layer 2 to Scene::Entity.
    struct PhysicsBodyState {
        float x = 0.0f;           // Top-left X world position
        float y = 0.0f;           // Top-left Y world position
        float velocityX = 0.0f;   // Horizontal linear velocity (pixels/sec)
        float velocityY = 0.0f;   // Vertical linear velocity (pixels/sec)
        float mass = 1.0f;        // Physical mass (> 0; ignored when isStatic is true)
        float restitution = 0.0f; // Bounciness coefficient [0.0 = inelastic .. 1.0 = elastic]
        bool isStatic = false;    // True for immovable obstacles (infinite mass / 0 inverse mass)
    };

    // =========================================================================
    // 2D Physics & Collision Subsystem (Layer 2: Domain Subsystems)
    // =========================================================================
    // Provides stateless geometric intersection tests, Minimum Translation Vector
    // (MTV) contact manifold generation, and impulse-based physical collision
    // resolution for Box vs Box, Circle vs Circle, and Mixed Circle vs Box pairs.
    class Physics2D {
    public:
        // Fast boolean overlap queries
        static bool CheckOverlap(const Rect2D& a, const Rect2D& b);
        static bool CheckOverlap(const Circle2D& a, const Circle2D& b);
        static bool CheckOverlap(const Circle2D& circle, const Rect2D& box);
        static bool CheckOverlap(const Rect2D& box, const Circle2D& circle);
        static bool CheckOverlap(const Shape2D& a, const Shape2D& b);

        // Narrow-phase contact manifold generation (Normal, Penetration Depth, Contact Point)
        static CollisionManifold ComputeBoxBox(const Rect2D& a, const Rect2D& b);
        static CollisionManifold ComputeCircleCircle(const Circle2D& a, const Circle2D& b);
        static CollisionManifold ComputeCircleBox(const Circle2D& circleA, const Rect2D& boxB);
        static CollisionManifold ComputeBoxCircle(const Rect2D& boxA, const Circle2D& circleB);
        static CollisionManifold ComputeManifold(const Shape2D& a, const Shape2D& b);

        // Resolves a collision between bodyA and bodyB using the contact manifold:
        // 1. Positional Correction (MTV push-out weighted by inverse mass)
        // 2. Impulse Velocity Resolution (momentum exchange & restitution bounce)
        static void ResolveCollision(PhysicsBodyState& bodyA,
                                     PhysicsBodyState& bodyB,
                                     const CollisionManifold& manifold);
    };

}
