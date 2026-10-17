#include "physics/physics2d.h"

namespace Talos::Physics {

    bool Physics2D::CheckOverlap(const Rect2D& a, const Rect2D& b) {
        // Separating Axis Theorem (SAT) simplified for unrotated 2D rectangles (AABB):
        // Two rectangles intersect if and only if there is no gap between them on
        // either the horizontal (X) axis or the vertical (Y) axis.
        //
        // 1. a.x < b.x + b.width   -> 'a' left edge is to the left of 'b' right edge
        // 2. a.x + a.width > b.x   -> 'a' right edge is to the right of 'b' left edge
        // 3. a.y < b.y + b.height  -> 'a' top edge is above 'b' bottom edge
        // 4. a.y + a.height > b.y  -> 'a' bottom edge is below 'b' top edge
        return (a.x < b.x + b.width)  &&
               (a.x + a.width > b.x)  &&
               (a.y < b.y + b.height) &&
               (a.y + a.height > b.y);
    }

}
