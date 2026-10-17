# Scene & Flat Entity Composition System

The **Scene & Entity System** (Layer 3) manages all objects in the game world, their capabilities, and their lifecycle during each frame. It is specifically designed to eliminate the two biggest workflow complaints found in traditional engines: **deep C++ inheritance hierarchies** and **fragile node-tree paths**.

---

## 1. Zero-Inheritance Flat Entities (`Entity` & `Components`)

### The Problem in Traditional Engines
* **Deep Inheritance (C++ OOP Trap):** When `Player` inherits from `Character`, which inherits from `PhysicsBody`, which inherits from `Node2D`, sharing behavior across unrelated types becomes rigid and error-prone.
* **Mandatory Child Node Bloat:** Requiring 3 to 5 auxiliary child nodes just to give an object a sprite, a collision box, and a text label clutters the scene hierarchy.

### The Talos Engine Solution
Every object in Talos Engine is an instance of a single flat structure (`Entity`) stored contiguously in memory (`std::vector<Entity>`). An `Entity` holds:
* **`id` (`EntityID`):** A unique 32-bit integer assigned automatically upon creation.
* **`name` (`std::string`):** A human-readable identifier for fast, path-independent lookups.
* **`active` (`bool`):** A master switch to enable or disable the entity across both update and render passes.
* **Direct Toggleable Components:**
  1. **`TransformComponent`:** Position `(x, y)` and dimensions `(width, height)`.
  2. **`SpriteComponent`:** Visual rectangle/sprite rendering (`enabled`, `color`).
  3. **`TextComponent`:** First-level typography (`enabled`, `content`, `fontSize`, `color`, `offsetX/Y`). All essential text properties live directly on the component without nested theme override menus.
  4. **`ColliderComponent`:** Solid 2D AABB collision (`enabled`, `isSolid`, `isStatic`) that automatically uses the entity's `TransformComponent` dimensions.
  5. **`MovementComponent`:** Velocity and optional built-in player input binding (`enabled`, `playerControlled`, `speed`, `velocityX/Y`).

Turning an ordinary visual box into a solid obstacle or a controllable character requires flipping a single boolean flag (`entity.collider.enabled = true;`) rather than restructuring a node tree.

---

## 2. Path-Independent Lookups (`Scene::FindEntity`)

In hierarchy-based engines, referencing an object via a relative path (such as `$Parent/Container/Panel/Player`) breaks immediately whenever you reorganize your scene.

In Talos Engine, `Scene` stores entities in a flat, cache-friendly list and provides direct lookup methods:
* `Scene::FindEntity("Player")` — Searches by human-readable name.
* `Scene::FindEntityById(id)` — Searches by persistent numeric `EntityID`.

Reordering entities or grouping them visually in the editor never invalidates code references.

---

## 3. Deterministic Frame Pipeline (`Scene::Update` & `Scene::Render`)

During each iteration of the main loop, `Scene` executes two strictly separated passes:

1. **`Scene::Update(float delta)`:**
   Iterates sequentially through all active entities with `movement.enabled`, samples directional input (if `playerControlled` is active), and resolves solid collisions along the $X$ and $Y$ axes via `Physics2D::CheckOverlap`.
2. **`Scene::Render()`:**
   Iterates sequentially through all active entities and submits enabled `SpriteComponent` and `TextComponent` data to `Renderer2D`. Because `Render()` is marked `const`, it is guaranteed at compile time never to mutate game state while drawing.
