#include "physics/physics2d.h"
#include <algorithm>
#include <cmath>

namespace Talos::Physics {

    // =========================================================================
    // 1. Fast Boolean Overlap Queries
    // =========================================================================

    bool Physics2D::CheckOverlap(const Rect2D& a, const Rect2D& b) {
        if (a.width <= 0.0f || a.height <= 0.0f || b.width <= 0.0f || b.height <= 0.0f) {
            return false;
        }
        return (a.x < b.x + b.width)  &&
               (a.x + a.width > b.x)  &&
               (a.y < b.y + b.height) &&
               (a.y + a.height > b.y);
    }

    bool Physics2D::CheckOverlap(const Circle2D& a, const Circle2D& b) {
        if (a.radius <= 0.0f || b.radius <= 0.0f) {
            return false;
        }
        float dx = b.centerX - a.centerX;
        float dy = b.centerY - a.centerY;
        float radiusSum = a.radius + b.radius;
        return (dx * dx + dy * dy) < (radiusSum * radiusSum);
    }

    bool Physics2D::CheckOverlap(const Circle2D& circle, const Rect2D& box) {
        if (circle.radius <= 0.0f || box.width <= 0.0f || box.height <= 0.0f) {
            return false;
        }
        float closestX = std::max(box.x, std::min(circle.centerX, box.x + box.width));
        float closestY = std::max(box.y, std::min(circle.centerY, box.y + box.height));
        float dx = closestX - circle.centerX;
        float dy = closestY - circle.centerY;
        return (dx * dx + dy * dy) < (circle.radius * circle.radius);
    }

    bool Physics2D::CheckOverlap(const Rect2D& box, const Circle2D& circle) {
        return CheckOverlap(circle, box);
    }

    bool Physics2D::CheckOverlap(const Shape2D& a, const Shape2D& b) {
        if (a.type == ColliderShape::Box && b.type == ColliderShape::Box) {
            return CheckOverlap(a.box, b.box);
        }
        if (a.type == ColliderShape::Circle && b.type == ColliderShape::Circle) {
            return CheckOverlap(a.circle, b.circle);
        }
        if (a.type == ColliderShape::Circle && b.type == ColliderShape::Box) {
            return CheckOverlap(a.circle, b.box);
        }
        return CheckOverlap(a.box, b.circle);
    }

    // =========================================================================
    // 2. Narrow-Phase Contact Manifold Generation
    // =========================================================================

    CollisionManifold Physics2D::ComputeBoxBox(const Rect2D& a, const Rect2D& b) {
        CollisionManifold m;
        if (!CheckOverlap(a, b)) {
            return m;
        }

        float halfWa = a.width * 0.5f;
        float halfHa = a.height * 0.5f;
        float halfWb = b.width * 0.5f;
        float halfHb = b.height * 0.5f;

        float centerXa = a.x + halfWa;
        float centerYa = a.y + halfHa;
        float centerXb = b.x + halfWb;
        float centerYb = b.y + halfHb;

        float dx = centerXb - centerXa;
        float dy = centerYb - centerYa;

        float overlapX = (halfWa + halfWb) - std::fabs(dx);
        float overlapY = (halfHa + halfHb) - std::fabs(dy);

        if (overlapX <= 0.0f || overlapY <= 0.0f) {
            return m;
        }

        m.colliding = true;
        if (overlapX < overlapY) {
            m.penetration = overlapX;
            m.normalX = (dx < 0.0f) ? -1.0f : 1.0f;
            m.normalY = 0.0f;
            m.contactX = (dx < 0.0f) ? a.x : (a.x + a.width);
            m.contactY = std::max(a.y, std::min(centerYb, a.y + a.height));
        } else {
            m.penetration = overlapY;
            m.normalX = 0.0f;
            m.normalY = (dy < 0.0f) ? -1.0f : 1.0f;
            m.contactX = std::max(a.x, std::min(centerXb, a.x + a.width));
            m.contactY = (dy < 0.0f) ? a.y : (a.y + a.height);
        }
        return m;
    }

    CollisionManifold Physics2D::ComputeCircleCircle(const Circle2D& a, const Circle2D& b) {
        CollisionManifold m;
        if (a.radius <= 0.0f || b.radius <= 0.0f) {
            return m;
        }

        float dx = b.centerX - a.centerX;
        float dy = b.centerY - a.centerY;
        float distSq = dx * dx + dy * dy;
        float radiusSum = a.radius + b.radius;

        if (distSq >= radiusSum * radiusSum) {
            return m;
        }

        m.colliding = true;
        float dist = std::sqrt(distSq);

        if (dist > 0.0001f) {
            m.normalX = dx / dist;
            m.normalY = dy / dist;
            m.penetration = radiusSum - dist;
            m.contactX = a.centerX + m.normalX * a.radius;
            m.contactY = a.centerY + m.normalY * a.radius;
        } else {
            // Concentric fallback: push apart horizontally
            m.normalX = 1.0f;
            m.normalY = 0.0f;
            m.penetration = radiusSum;
            m.contactX = a.centerX;
            m.contactY = a.centerY;
        }
        return m;
    }

    CollisionManifold Physics2D::ComputeCircleBox(const Circle2D& circleA, const Rect2D& boxB) {
        CollisionManifold m;
        if (circleA.radius <= 0.0f || boxB.width <= 0.0f || boxB.height <= 0.0f) {
            return m;
        }

        float minX = boxB.x;
        float maxX = boxB.x + boxB.width;
        float minY = boxB.y;
        float maxY = boxB.y + boxB.height;

        float cx = circleA.centerX;
        float cy = circleA.centerY;

        // Clamp the circle's center to the AABB perimeter/interior to find the closest box point
        float closestX = std::max(minX, std::min(cx, maxX));
        float closestY = std::max(minY, std::min(cy, maxY));

        bool centerInside = (cx >= minX && cx <= maxX && cy >= minY && cy <= maxY);

        // Vector pointing from circleA's center (A) toward the closest point on boxB (B)
        float dx = closestX - cx;
        float dy = closestY - cy;
        float distSq = dx * dx + dy * dy;

        if (!centerInside) {
            if (distSq >= circleA.radius * circleA.radius) {
                return m;
            }

            float dist = std::sqrt(distSq);
            if (dist > 0.0001f) {
                m.colliding = true;
                m.normalX = dx / dist;
                m.normalY = dy / dist;
                m.penetration = circleA.radius - dist;
                m.contactX = closestX;
                m.contactY = closestY;
                return m;
            }
        }

        // Deep Penetration Case: Circle center is inside (or exactly on the edge of) the Box.
        // Find the nearest face of boxB so circleA is cleanly ejected along the shortest axis.
        float distLeft   = cx - minX;
        float distRight  = maxX - cx;
        float distTop    = cy - minY;
        float distBottom = maxY - cy;

        float minDist = distLeft;
        m.normalX  = 1.0f;
        m.normalY  = 0.0f;
        m.contactX = minX;
        m.contactY = cy;

        if (distRight < minDist) {
            minDist    = distRight;
            m.normalX  = -1.0f;
            m.normalY  = 0.0f;
            m.contactX = maxX;
            m.contactY = cy;
        }
        if (distTop < minDist) {
            minDist    = distTop;
            m.normalX  = 0.0f;
            m.normalY  = 1.0f;
            m.contactX = cx;
            m.contactY = minY;
        }
        if (distBottom < minDist) {
            minDist    = distBottom;
            m.normalX  = 0.0f;
            m.normalY  = -1.0f;
            m.contactX = cx;
            m.contactY = maxY;
        }

        m.colliding = true;
        m.penetration = circleA.radius + minDist;
        return m;
    }

    CollisionManifold Physics2D::ComputeBoxCircle(const Rect2D& boxA, const Circle2D& circleB) {
        // Compute manifold with circle as A and box as B, then invert the normal
        // so that (normalX, normalY) points from boxA (A) toward circleB (B).
        CollisionManifold m = ComputeCircleBox(circleB, boxA);
        if (m.colliding) {
            m.normalX = -m.normalX;
            m.normalY = -m.normalY;
        }
        return m;
    }

    CollisionManifold Physics2D::ComputeManifold(const Shape2D& a, const Shape2D& b) {
        if (a.type == ColliderShape::Box && b.type == ColliderShape::Box) {
            return ComputeBoxBox(a.box, b.box);
        }
        if (a.type == ColliderShape::Circle && b.type == ColliderShape::Circle) {
            return ComputeCircleCircle(a.circle, b.circle);
        }
        if (a.type == ColliderShape::Circle && b.type == ColliderShape::Box) {
            return ComputeCircleBox(a.circle, b.box);
        }
        return ComputeBoxCircle(a.box, b.circle);
    }

    // =========================================================================
    // 3. Physical Collision Resolution (MTV Positional Correction + Impulse)
    // =========================================================================

    void Physics2D::ResolveCollision(PhysicsBodyState& bodyA,
                                     PhysicsBodyState& bodyB,
                                     const CollisionManifold& manifold) {
        if (!manifold.colliding || manifold.penetration <= 0.0f) {
            return;
        }

        float invMassA = (bodyA.isStatic || bodyA.mass <= 0.0001f) ? 0.0f : (1.0f / bodyA.mass);
        float invMassB = (bodyB.isStatic || bodyB.mass <= 0.0001f) ? 0.0f : (1.0f / bodyB.mass);
        float totalInvMass = invMassA + invMassB;

        if (totalInvMass <= 0.0f) {
            return;
        }

        // 1. Positional Correction (Minimum Translation Vector weighted by inverse mass)
        float ratioA = invMassA / totalInvMass;
        float ratioB = invMassB / totalInvMass;

        bodyA.x -= manifold.normalX * manifold.penetration * ratioA;
        bodyA.y -= manifold.normalY * manifold.penetration * ratioA;
        bodyB.x += manifold.normalX * manifold.penetration * ratioB;
        bodyB.y += manifold.normalY * manifold.penetration * ratioB;

        // 2. Impulse-Based Velocity Resolution along Collision Normal
        float rvX = bodyB.velocityX - bodyA.velocityX;
        float rvY = bodyB.velocityY - bodyA.velocityY;
        float velAlongNormal = rvX * manifold.normalX + rvY * manifold.normalY;

        // Do not apply velocity impulse if the bodies are already separating
        if (velAlongNormal > 0.0f) {
            return;
        }

        float e = std::max(bodyA.restitution, bodyB.restitution);
        e = std::max(0.0f, std::min(1.0f, e));

        // Suppress micro-bounces at very low impact speeds to prevent resting jitter
        if (std::fabs(velAlongNormal) < 8.0f) {
            e = 0.0f;
        }

        float j = -(1.0f + e) * velAlongNormal / totalInvMass;
        float impulseX = j * manifold.normalX;
        float impulseY = j * manifold.normalY;

        bodyA.velocityX -= impulseX * invMassA;
        bodyA.velocityY -= impulseY * invMassA;
        bodyB.velocityX += impulseX * invMassB;
        bodyB.velocityY += impulseY * invMassB;
    }

}
