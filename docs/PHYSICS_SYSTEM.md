# 2D Physics & Collision Subsystem (`Physics2D`)

The **2D Physics Subsystem** sits in the Domain Subsystems Layer (Layer 2) and provides fast, deterministic geometric intersection queries, narrow-phase contact manifold generation, and physical collision resolution for **`Box` (AABB)** and **`Circle`** shapes in 2D space.

---

## 1. Strict Decoupling from Scene Entities

To preserve unidirectional layer dependencies, `Physics2D` has **zero knowledge** of `Entity`, `Scene`, or gameplay components. Instead, it operates exclusively on pure mathematical primitives (`physics/physics2d.h`):

* **`ColliderShape`:** Enumerates `Box = 0` (Axis-Aligned Bounding Box) and `Circle = 1`.
* **`Rect2D` (Axis-Aligned Bounding Box / AABB):** Defined by a top-left origin `(x, y)` and dimensions `(width, height)`.
* **`Circle2D`:** Defined by a world-space center `(centerX, centerY)` and `radius`.
* **`Shape2D`:** Unified 2D collider primitive (`Shape2D::MakeBox(...)` and `Shape2D::MakeCircle(...)`) used to dispatch narrow-phase queries across any shape pair (`Box vs Box`, `Circle vs Circle`, `Circle vs Box`, and `Box vs Circle`).
* **`CollisionManifold`:** Narrow-phase contact result containing `colliding`, unit collision normal `(normalX, normalY)` pointing from shape **A** toward shape **B**, `penetration` depth (MTV magnitude), and world-space contact point `(contactX, contactY)`.
* **`PhysicsBodyState`:** Lightweight kinematic/dynamic state (`x`, `y`, `velocityX`, `velocityY`, `mass`, `restitution`, `isStatic`) passed into `Physics2D::ResolveCollision`.

---

## 2. Overlap Queries (`Physics2D::CheckOverlap`)

`Physics2D` provides fast boolean intersection tests for all shape combinations:

1. **`Box vs Box` (`Rect2D` vs `Rect2D`):**
   Evaluates projection overlap on both $X$ and $Y$ axes via the Separating Axis Theorem (SAT).
2. **`Circle vs Circle` (`Circle2D` vs `Circle2D`):**
   Compares squared center-to-center distance $d^2 = \Delta x^2 + \Delta y^2$ against $(r_A + r_B)^2$ without requiring a square root.
3. **`Mixed Circle vs Box` (`Circle2D` vs `Rect2D` & `Rect2D` vs `Circle2D`):**
   Clamps the circle center $(c_x, c_y)$ to the closest point $(p_x, p_y)$ inside the box bounds $[b_x, b_x + b_w] \times [b_y, b_y + b_h]$ and tests whether $(c_x - p_x)^2 + (c_y - p_y)^2 < r^2$.

---

## 3. Narrow-Phase Contact Manifolds (`Physics2D::ComputeManifold`)

* **`ComputeBoxBox(a, b)`:** Computes overlap along both axes (`overlapX`, `overlapY`) and selects the axis of minimum penetration as the Minimum Translation Vector (MTV).
* **`ComputeCircleCircle(a, b)`:** Computes the unit vector from center $A$ to center $B$ and penetration depth $(r_A + r_B) - d$.
* **`ComputeCircleBox(circle, box)` & `ComputeBoxCircle(box, circle)`:**
  Handles both **exterior contacts** and **deep interior penetration**:
  1. **Exterior Contact ($d^2 > 0$):** Clamps the circle center to the nearest point on the box perimeter. The contact normal points from the circle center toward the clamped perimeter point with penetration $r - d$, enabling smooth rounded corner sliding when a circle brushes past a box corner or vice versa.
  2. **Interior Center Penetration ($d^2 = 0$):** When the circle's center lies strictly inside the box, computes the distance to all four box faces (`left`, `right`, `top`, `bottom`), selects the nearest face as the exit normal, and sets penetration to $r + \text{distToNearestFace}$ to eject the circle cleanly without tunneling.

---

## 4. Physical Collision Resolution (`Physics2D::ResolveCollision`)

Given two bodies `bodyA` and `bodyB` and their `CollisionManifold`:

1. **Inverse-Mass Weighted Positional Correction (MTV):**
   Separates overlapping bodies along `(normalX, normalY)` proportionally to their inverse masses ($w_A = m_A^{-1}$, $w_B = m_B^{-1}$, with $w = 0$ for `isStatic == true`):
   $$\Delta \mathbf{x}_A = -\mathbf{n} \cdot p \cdot \frac{w_A}{w_A + w_B}, \qquad \Delta \mathbf{x}_B = +\mathbf{n} \cdot p \cdot \frac{w_B}{w_A + w_B}$$
2. **Impulse Velocity Resolution with Restitution ($e$):**
   Computes relative velocity along the contact normal $v_{\text{rel}} = (\mathbf{v}_B - \mathbf{v}_A) \cdot \mathbf{n}$. If the bodies are approaching ($v_{\text{rel}} < 0$), applies the normal impulse scalar:
   $$j = \frac{-(1 + e) \, v_{\text{rel}}}{w_A + w_B}, \qquad e = \max(e_A, e_B)$$
   This produces natural wall sliding when $e = 0$ and realistic elastic bouncing between circles and boxes when $e > 0$.


