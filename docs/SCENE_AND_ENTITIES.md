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
* **`tag` (`std::string`):** Semantic category label (e.g., `"Player"`, `"Enemy"`, `"Coin"`, `"Wall"`) used by scripts, collision events, and tag-based queries (`entity.HasTag("Enemy")`).
* **`active` (`bool`):** A master switch to enable or disable the entity across both update and render passes.
* **Direct Toggleable Components (Only `TransformComponent` is active on a newly created entity; all other components default to `enabled = false` and are attached on demand via the Inspector's `+ Add Component` menu or in code):**
  1. **`TransformComponent` (Fundamental):** Position `(x, y)` and dimensions `(width, height)`, always present on every entity.
  2. **`SpriteComponent`:** Visual rectangle/sprite rendering (`enabled`, `texturePath`, `color`), explicit depth sorting (`sortingLayer`, `zOrder`), UV sub-rectangle & mirror flipping (`sourceX`, `sourceY`, `sourceW`, `sourceH`, `flipX`, `flipY`), and integrated 2D frame-by-frame animation (`frameCount`, `currentFrame`, `fps`, `loop`, `isPlaying`, `frameTimer`, and `SyncFrameUV`).
  3. **`TextComponent`:** First-level typography (`enabled`, `content`, `fontSize`, `color`, `offsetX/Y`). All essential text properties live directly on the component without nested theme override menus.
  4. **`ColliderComponent`:** Solid 2D collision or non-blocking sensor/trigger area (`enabled`, `shape` = `Box` or `Circle`, `radius`, `restitution`, `isSolid`, `isStatic`, `isTrigger`) that automatically fits the entity's `TransformComponent` dimensions (or uses an explicit custom circle radius when `radius > 0`).
  5. **`MovementComponent`:** Velocity, rigid-body properties, and optional built-in player input binding (`enabled`, `playerControlled`, `speed`, `mass`, `gravityScale`, `velocityX/Y`).
  6. **`CameraComponent`:** Configurable 2D runtime game camera (`enabled`, `isPrimary`, `followTarget`, `smoothFollow`, `targetName`, `followSpeed`, `zoom`, `offsetX`, `offsetY`) supporting both fixed framing and frame-rate-independent smooth target tracking.
  7. **`AudioComponent` (`AudioSource`):** Configurable 2D entity sound effect (`Sound` in RAM) or streaming music (`Music` stream) source (`enabled`, `audioPath`, `isMusic`, `playOnStart`, `loop`, `playOnCollision`, `volume`, `pitch`, `pan`).

Attaching a visual sprite, text label, solid obstacle, pass-through sensor/trigger zone, controllable character movement, runtime camera, or audio source is done either from the **`+ Add Component`** dropdown at the bottom of the Inspector or in code (`entity.audio.enabled = true;`).

---

## 2. Path-Independent Lookups (`Scene::FindEntity` & Tag Queries)

In hierarchy-based engines, referencing an object via a relative path (such as `$Parent/Container/Panel/Player`) breaks immediately whenever you reorganize your scene.

In Talos Engine, `Scene` stores entities in a flat, cache-friendly list and provides direct lookup methods:
* `Scene::FindEntity("Player")` — Searches by human-readable name.
* `Scene::FindEntityById(id)` — Searches by persistent numeric `EntityID`.
* `Scene::FindEntityByTag("Player")` — Returns the first entity matching the semantic tag.
* `Scene::FindEntitiesByTag("Enemy")` — Returns all entities matching the semantic tag.
* `Scene::FindActiveCameraEntity()` — Resolves the primary active `CameraComponent` entity in the scene.
* `Scene::DuplicateEntity(sourceId, offsetX, offsetY)` — Clones an existing entity with all its components, assigns a new unique `EntityID` and sequential name (e.g., `"Hero"` $\rightarrow$ `"Hero_2"`), offsets its position, and resets transient runtime flags.

Reordering entities or grouping them visually in the editor never invalidates code references.

---

## 3. Deterministic Frame Pipeline (`Scene::Update`, `Scene::UpdateAnimations` & `Scene::Render`)

During each iteration of the main loop, `Scene` executes strictly separated passes:

1. **`Scene::Update(float delta)`:**
   - Calls `Scene::UpdateAnimations(delta)` to advance frame-based sprite animations (`frameCount > 1` and `isPlaying == true`).
   - Performs semi-implicit Euler integration on all active dynamic entities with `movement.enabled` (sampling directional input if `playerControlled` is active, integrating `gravityScale`, or applying top-down linear damping).
   - Runs an iterative narrow-phase collision solver (`Physics2D::ComputeManifold` + `Physics2D::ResolveCollision`) across all active solid collider pairs (`isSolid && !isTrigger`) for `Box vs Box`, `Circle vs Circle`, and mixed `Circle vs Box`.
   - Tracks solid collision pairs across frames, emits `CollisionEvent` transitions (`CollisionEventState::Enter`, `Stay`, and `Exit`) complete with entity IDs, names, tags (`entityATag`, `entityBTag`), contact normal, penetration depth, and world contact point, notifies the scripting/listener system via `CollisionCallback` (`Scene::SetCollisionCallback`), and supports tag-aware queries (`Scene::IsColliding`, `Scene::IsCollidingWithTag`).
   - Evaluates non-blocking **Sensor / Trigger** overlaps (`isTrigger == true`) via `Physics2D::CheckOverlap(shapeA, shapeB)`, tracks active overlaps across frames, emits `TriggerOverlapEvent` transitions (`TriggerEventState::Enter`, `Stay`, and `Exit`) enriched with `triggerTag` and `otherTag`, dispatches `TriggerCallback` toward scripts/listeners, and supports `Scene::IsTriggerOverlapping` / `Scene::IsTriggerOverlappingWithTag`.
   - Advances all active `CameraComponent` instances (resolving fixed positions or smoothly interpolating `(currentX, currentY)` toward `targetName` after movement has settled) and records telemetry into `SceneDiagnostics::camera`.
   - Evaluates all active `AudioComponent` instances (triggering playback on `playOnStart`, re-triggering on `loop`, firing on `CollisionEventState::Enter` / `TriggerEventState::Enter` when `playOnCollision` is enabled, and live-updating `volume`, `pitch`, and `pan` via `AudioSystem`) and records telemetry into `SceneDiagnostics::audio`.
2. **`Scene::UpdateAnimations(float delta)`:**
   Advances `frameTimer` for all active entities with multi-frame sprite sheets (`frameCount > 1`, `isPlaying == true`, `fps > 0`), stepping `currentFrame` at `1.0f / fps` intervals, wrapping on `loop = true` or clamping and auto-stopping at `frameCount - 1` when `loop = false`, and synchronizing the UV sub-rectangle via `SpriteComponent::SyncFrameUV`. Also invoked by the Editor in Edit Mode for live viewport animation preview.
3. **`Scene::Render()`:**
   Sorts active entities deterministically by `(sortingLayer, zOrder)` via `std::stable_sort` and submits enabled `SpriteComponent` (textured sprites, solid boxes, or solid circles) and `TextComponent` data to `Renderer2D`. Because `Render()` is marked `const`, it is guaranteed at compile time never to mutate game state while drawing.


