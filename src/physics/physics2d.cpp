#include "physics/physics2d.h"

namespace Talos::Physics {

    bool Physics2D::CheckOverlap(const Rect2D& a, const Rect2D& b) {
        return (a.x < b.x + b.width)  &&
               (a.x + a.width > b.x)  &&
               (a.y < b.y + b.height) &&
               (a.y + a.height > b.y);
    }

}

