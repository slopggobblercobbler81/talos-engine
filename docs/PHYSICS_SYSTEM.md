# 2D Physics & Collision Subsystem (`Physics2D`)

The **2D Physics Subsystem** sits in the Domain Subsystems Layer (Layer 2) and provides fast, deterministic geometric intersection queries for 2D space.

---

## 1. Strict Decoupling from Scene Entities

To preserve unidirectional layer dependencies, `Physics2D` has **zero knowledge** of `Entity`, `Scene`, or gameplay components. Instead, it operates exclusively on a pure mathematical primitive:

* **`Rect2D` (Axis-Aligned Bounding Box / AABB):**
  Defined by a top-left origin `(x, y)` and dimensions `(width, height)`. Any higher-level system (such as the `Scene` layer) constructs temporary `Rect2D` primitives from entity transforms and queries `Physics2D`.

---

## 2. AABB Intersection Mathematics (`Physics2D::CheckOverlap`)

Two unrotated rectangles `A` and `B` overlap in 2D space if and only if their projections overlap on **both** the horizontal ($X$) axis and the vertical ($Y$) axis (a direct application of the **Separating Axis Theorem**):

```cpp
(a.x < b.x + b.width)  && // A's left edge is left of B's right edge
(a.x + a.width > b.x)  && // A's right edge is right of B's left edge
(a.y < b.y + b.height) && // A's top edge is above B's bottom edge
(a.y + a.height > b.y)    // A's bottom edge is below B's top edge
```

If any single condition evaluates to `false`, a separating gap exists between the two boxes and they cannot be colliding. Because this check requires only four floating-point comparisons, thousands of pairs can be evaluated per millisecond.

---

## 3. Separated-Axis Collision Resolution (Sliding Along Walls)

When moving a solid entity in `Scene::Update`, movement is resolved in **two independent steps** rather than a single diagonal step:

1. **Horizontal Step ($X$):**
   Compute the candidate position `nextX = x + velocityX * delta`. Construct a candidate `Rect2D` at `(nextX, y)` and test against all solid colliders via `Physics2D::CheckOverlap`. If no collision occurs, commit `x = nextX`.
2. **Vertical Step ($Y$):**
   Compute the candidate position `nextY = y + velocityY * delta`. Construct a candidate `Rect2D` at `(x, nextY)` and test against all solid colliders. If no collision occurs, commit `y = nextY`.

### Why Separate X and Y?
If a player holds both **Right (`D`)** and **Up (`W`)** while pushing against a vertical wall on their right:
* A combined diagonal check would detect a collision and cancel **both** $X$ and $Y$ movement, causing the character to "stick" to the wall.
* Separating $X$ and $Y$ blocks only the horizontal $X$ step while allowing the vertical $Y$ step to succeed, producing smooth, natural wall-sliding behavior out of the box.

