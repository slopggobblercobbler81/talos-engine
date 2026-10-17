# Viewport Diagnostic Overlay System (`F3`)

## Overview
The **Viewport Diagnostic Overlay** provides real-time, per-component performance and memory telemetry directly inside the central 2D Viewport while running a game simulation in **Talos Engine**.

It measures and displays the exact runtime consumption of each Engine Component (`Transform 2D`, `Movement 2D`, `Collider 2D`, `Sprite 2D`, and `Text Label`), including high-precision CPU execution time in microseconds (`us`), active and total RAM allocation in bytes, GPU VRAM footprint for textures and TrueType font atlases, frame operation counts, and a per-component breakdown for the currently selected entity.

---

## Activation & Persistence Rules (`F3`)

1. **Exclusive Play-Mode Toggle (`F3`):**
   - Pressing `F3` toggles the diagnostic overlay **only while the simulation is running** (`m_isPlaying == true`).
   - Pressing `F3` while in **Edit Mode** (`m_isPlaying == false`) has no effect.
2. **Play-Mode Visibility:**
   - The overlay is rendered inside the Viewport only when both `m_isPlaying` and `m_diagnosticOverlayEnabled` are `true`.
3. **Cross-Session State Persistence:**
   - Stopping the simulation (`STOP GAME`) does **not** reset `m_diagnosticOverlayEnabled`.
   - If the user enables the overlay with `F3` during Play Mode and stops the simulation without turning it off, the overlay automatically reappears the moment Play Mode is started again (`PLAY GAME`).

---

## Per-Component Telemetry Metrics

Every frame during Play Mode, `Scene::Update(float delta)` and `Scene::Render()` instrument each component pass using `std::chrono::steady_clock` and exact memory accounting (`ComponentDiagnostics` / `SceneDiagnostics`):

| Component | CPU Time (`us` / `ms`) | RAM (`Active / Total`) | GPU VRAM | Frame Operations (`OPS`) |
| :--- | :--- | :--- | :--- | :--- |
| **Transform 2D** | World-bounds clamping pass timing | `sizeof(TransformComponent)` (`16 B` per entity) | `0 B` | Clamped entity bounds count (`bounds`) |
| **Movement 2D** | Velocity & input integration pass timing | `sizeof(MovementComponent)` (`16 B` per entity) | `0 B` | Moving entity count (`moving`) |
| **Collider 2D** | AABB overlap detection & resolution timing | `sizeof(ColliderComponent)` (`3 B` per entity) | `0 B` | Pairwise AABB checks (`AABB`) |
| **Sprite 2D** | 2D texture/quad submission pass timing | `sizeof(SpriteComponent) + texturePath.capacity()` | Unique active GPU texture bytes (`w * h * 4`) | Rendered sprite draw calls (`draws`) |
| **Text Label** | TrueType glyph submission pass timing | `sizeof(TextComponent) + content.capacity()` | Unique active GPU font atlas bytes (`w * h * 4`) | Rendered text draw calls (`draws`) |

### Selected Entity Breakdown
At the bottom of the diagnostic overlay, selecting any entity in the Hierarchy panel displays its exact individual component footprint in real time:
- Exact RAM bytes per component (`Trans`, `Move`, `Coll`, `Sprite`, `Text`) including dynamic string capacities.
- Exact GPU VRAM bytes referenced by that entity's active `SpriteComponent` texture and `TextComponent` TrueType font atlas.
- Component active/inactive state (`ON` / `OFF`).
