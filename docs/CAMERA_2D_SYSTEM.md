# Interactive 2D Camera System (Pan & Zoom)

## Overview

The **Interactive 2D Camera System** provides a responsive, hardware-accelerated 2D viewport camera for navigating the game world inside the Editor. It decouples world-space coordinates from screen-space window pixels, allowing developers to pan freely across an infinite 2D canvas, zoom smoothly toward the mouse cursor, focus on specific entities, and manipulate objects accurately at any zoom scale.

---

## Architectural Design

In compliance with the engine's strict unidirectional dependency graph (`Core` <- `Renderer` <- `Scene` <- `Editor`), the camera system is split across three layers:

1. **Input Layer (`Core::Input`):**
   - Exposes `GetMouseWheelMove()` to report vertical scroll wheel steps per frame.
   - Provides key bindings for camera shortcuts such as `Key::Space` (hand-pan modifier) and `Key::F` (focus selected entity).

2. **Rendering Layer (`Renderer::Camera2DView` & `Renderer::Renderer2D`):**
   - `Camera2DView` is a pure mathematical structure holding:
     - `targetX`, `targetY`: The world-space coordinate anchored at the camera's screen offset.
     - `offsetX`, `offsetY`: The screen-space pixel pivot (typically the center of the central Editor viewport).
     - `zoom`: The uniform scale factor (`1.0f` = 100%, clamped between `0.20f` and `5.00f`).
   - Provides bidirectional analytical coordinate transformations (`ScreenToWorld` and `WorldToScreen`).
   - `Renderer2D::BeginCamera2D` and `Renderer2D::EndCamera2D` push and pop the OpenGL 2D projection matrix so all subsequent world draws are automatically transformed on the GPU.
   - `Renderer2D::BeginViewportScissor` and `Renderer2D::EndViewportScissor` clip all world-space rendering strictly to the central viewport rectangle so zoomed or panned objects never bleed behind the side panels or top toolbar.

3. **Editor Layer (`Editor::EditorApp`):**
   - Owns the active `Camera2DView` instance (`m_camera`).
   - Processes mouse wheel zoom, panning gestures, entity picking in world space, infinite grid rendering, and the viewport camera HUD overlay.

---

## Coordinate Space Transformations

With rotation fixed at `0` for 2D editor navigation, the transformation between screen pixels $(S_x, S_y)$ and world units $(W_x, W_y)$ is defined as:

### World to Screen
$$S_x = (W_x - \text{target}_x) \cdot \text{zoom} + \text{offset}_x$$
$$S_y = (W_y - \text{target}_y) \cdot \text{zoom} + \text{offset}_y$$

### Screen to World
$$W_x = \frac{S_x - \text{offset}_x}{\text{zoom}} + \text{target}_x$$
$$W_y = \frac{S_y - \text{offset}_y}{\text{zoom}} + \text{target}_y$$

---

## Key Mechanics

### 1. Zoom-to-Cursor Anchoring
When the user scrolls the mouse wheel inside the central viewport:
1. The system samples the exact world coordinate underneath the mouse cursor **before** changing the zoom level:
   $$\mathbf{W}_{\text{before}} = \text{ScreenToWorld}(M_x, M_y)$$
2. The zoom factor is multiplied or divided by `1.12f` per wheel notch and clamped to `[0.20f, 5.00f]`.
3. The camera target is immediately solved so that $\mathbf{W}_{\text{before}}$ remains locked under the current mouse pixel $(M_x, M_y)$:
   $$\text{target}_x = W_{\text{before}, x} - \frac{M_x - \text{offset}_x}{\text{zoom}_{\text{new}}}$$
   $$\text{target}_y = W_{\text{before}, y} - \frac{M_y - \text{offset}_y}{\text{zoom}_{\text{new}}}$$

### 2. Multi-Gesture Camera Panning
The user can pan the camera while inside the viewport using any of three standard industry gestures:
- **Middle Mouse Button Drag**
- **Right Mouse Button Drag**
- **`Space` + Left Mouse Button Drag** (Hand Tool style)

During an active pan, screen-space mouse deltas $(\Delta M_x, \Delta M_y)$ are converted into inverse world-space target displacements scaled by the current zoom factor:
$$\text{target}_x \mathrel{-}= \frac{\Delta M_x}{\text{zoom}}, \qquad \text{target}_y \mathrel{-}= \frac{\Delta M_y}{\text{zoom}}$$

### 3. Infinite Frustum-Culled World Grid
Instead of drawing a fixed loop over window pixels, the Editor computes the visible world-space bounding box of the viewport each frame using `ScreenToWorld`:
- Snap the top-left visible world coordinate to the nearest multiple of the grid step (`50` world units).
- Iterate only across the visible world span to draw minor (`50` unit) and major (`250` unit) grid lines.
- Render dedicated **World Origin Axes** ($X = 0$ in subtle green, $Y = 0$ in subtle red) and an origin badge `(0,0)` so the user always knows where the scene origin lies.

### 4. Zoom-Independent Gizmo Thickness
Selection outlines and physics collider wireframes are drawn inside `BeginCamera2D`, meaning the GPU scales their geometry by `m_camera.zoom`. To prevent outlines from becoming overly thick when zoomed in or disappearing when zoomed out, stroke thickness is inversely scaled by `1.0f / m_camera.zoom`.

### 5. Viewport Camera HUD Overlay
A compact floating HUD bar in the bottom-left corner of the viewport displays:
- Current **Camera Target** coordinates (`Cam: (X, Y)`).
- Current **Zoom Percentage** (`Zoom: 100%`).
- Interactive **`Reset`** button to restore `100%` zoom centered on the default canvas.
- Interactive **`Focus [F]`** button (and `F` keyboard shortcut) to smoothly snap the camera target to the currently selected entity.

---

## Runtime Game Camera (`Scene::CameraComponent`)

In addition to the Editor navigation camera, Talos Engine provides an entity-integrated **Runtime Game Camera (`CameraComponent`)** used during Play Mode and in the exported standalone game.

### 1. Component Properties (`scene/components.h`)
- **`enabled` (`bool`):** Enables or disables the camera on the entity.
- **`isPrimary` (`bool`):** Marks this camera as the primary active camera when multiple cameras exist in the scene.
- **`followTarget` (`bool`):**
  - `false` (**Fixed Mode**): Locks the camera center to the owning entity's center plus `(offsetX, offsetY)`.
  - `true` (**Follow Target Mode**): Tracks a target entity by name (`targetName`), or tracks the owning entity itself if `targetName` is empty or matches the entity's own name.
- **`smoothFollow` (`bool`):**
  - `true`: Applies frame-rate-independent exponential damping toward the target center:
    $$\alpha = \min(1.0, \text{followSpeed} \cdot \Delta t)$$
    $$\text{current} \mathrel{+}= (\text{desired} - \text{current}) \cdot \alpha$$
  - `false`: Snaps instantaneously to the target center each frame.
- **`targetName` (`std::string`):** Name of the target entity to follow (e.g., `"Player"`).
- **`followSpeed` (`float`):** Damping speed factor (`0.5` to `25.0`, default `8.0`).
- **`zoom` (`float`):** Runtime projection zoom multiplier (`0.25x` to `4.00x`, default `1.00x`).
- **`offsetX`, `offsetY` (`float`):** World-space look-ahead or framing offset relative to the tracked center.

### 2. Primary Camera Resolution (`Scene::ResolveRuntimeCamera`)
- `Scene::FindActiveCameraEntity()` returns the first active entity with `camera.enabled && camera.isPrimary` (or falls back to the first active entity with `camera.enabled`).
- `Scene::ResolveRuntimeCamera(viewX, viewY, viewWidth, viewHeight, outCamera)` builds a `Renderer::Camera2DData` centered on the viewport rectangle with the active camera's `(currentX, currentY)` target and `zoom`.
- During **Play Mode**, `CanvasViewport::RenderWorld` automatically switches from the Editor camera to `Scene::ResolveRuntimeCamera(...)` whenever an active `CameraComponent` is present in the scene, displaying a live status badge (`RUNTIME CAMERA: ...`) in the bottom-left corner of the viewport.

### 3. Edit Mode Frustum Gizmo & Inspector Controls
- When an entity with `camera.enabled` is selected in **Edit Mode**, `CanvasViewport::RenderSelectionGizmo` draws a cyan **Runtime Camera Frustum Frame** representing the exact world area (`viewWidth / zoom` $\times$ `viewHeight / zoom`) that will be visible in Play Mode, complete with a center reticle, offset guide line, and a `CAM: Fixed/Follow` header badge.
- In the **Hierarchy (`SceneTreeDock`)**, entities with an active camera display a cyan `CAM` badge.
- In the **First-Level Inspector (`InspectorDock`)**, the `8. CAMERA 2D (Runtime View)` section exposes `[Fixed]` / `[Follow]` mode buttons, `Primary` toggle, `Target` text field with `[Self]` / `[Player]` quick presets, `Smooth Follow (Damping)` toggle, `Follow Spd`, `Zoom (%)` with `[100%]` reset, and `Offset X / Y` sliders.

