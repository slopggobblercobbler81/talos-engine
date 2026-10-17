# Visual Editor & First-Level Inspector System (`Editor`)

The **Editor System** sits at the highest layer of Talos Engine (Layer 5). It acts as a visual authoring environment that inspects and mutates a `Scene` instance without introducing any reverse dependencies into the core runtime layers.

---

## 1. Architectural Separation (Editor vs. Runtime)

A common mistake in game engine design is tangling editor UI logic inside entity or rendering classes. Talos Engine enforces strict top-down isolation:
* The `Core`, `Renderer2D`, `Physics2D`, and `Scene` layers have **zero knowledge** that `Editor` exists.
* The `Editor` builds its entire user interface (buttons, checkboxes, sliders, color swatches, and text input boxes) purely on top of the public `Core::Input` and `Renderer::Renderer2D` interfaces.
* Because there is no third-party UI framework bloat, the entire editor compiles in under two seconds and runs at locked frame rates with minimal memory overhead.

---

## 2. Instant Play / Stop State Snapshotting (Zero-Wait Iteration)

One of the most frustrating bottlenecks in heavyweight engines is waiting several seconds for domain reloads when pressing **Play**, or losing editor object positions when testing gameplay.

Talos Engine solves this using **In-Memory Flat State Snapshotting**:
1. **`StartPlayMode`:**
   Because every `Entity` in `Scene` is a flat, self-contained data structure stored in a contiguous `std::vector<Entity>`, clicking **PLAY GAME** copies the entire scene state into `m_sceneSnapshot` in microseconds (`0.0001s`) and begins executing `Scene::Update(delta)` every frame.
2. **`StopPlayMode`:**
   Clicking **STOP GAME** immediately assigns `m_sceneSnapshot` back into the active `Scene`. Every entity instantly returns to its exact pre-Play position, color, and state with **zero loading bars**.

---

## 3. Viewport Mouse Picking & Direct Dragging

When in **Edit Mode** (`!IsPlaying()`):
* **Front-to-Back Hit Testing:** Clicking inside the central 2D viewport iterates backward through the entity list so visually topmost objects are selected first.
* **Text-Only Entity Picking:** If an entity has its `SpriteComponent` disabled and only displays a `TextComponent` (such as a UI banner or title), the Editor dynamically calculates the bounding box of the text string using `Renderer2D::MeasureTextWidth` so text objects can be clicked and dragged just as easily as sprites.
* **Smooth Drag-Offset Tracking:** When an entity is clicked, the Editor records `m_dragOffsetX = mouseX - entity.x` and `m_dragOffsetY = mouseY - entity.y`. While the mouse button remains held down, the entity follows the cursor smoothly without snapping its top-left corner to the cursor tip.

---

## 4. The "First-Level Inspector" Philosophy

To eliminate the frustration of unfolding three or four nested sub-menus just to change a font color or resize an object, the right-hand Inspector panel exposes all high-frequency properties on the **First Level**:

1. **Identity & Lifecycle:** Direct `Active` checkbox and live-editable `Name` text field.
2. **Transform 2D:** Direct sliders for `Pos X`, `Pos Y`, `Width`, and `Height`.
3. **Sprite Component:** Toggleable checkbox + **8 one-click preset color swatches** + direct `R`, `G`, `B` sliders.
4. **Text Label Component:** Toggleable checkbox + live keyboard text input box + `Font Size` slider + `Offset Y` slider + **8 one-click preset color swatches** + direct `R`, `G`, `B` sliders.
5. **Collider 2D & Movement 2D:** Direct single-click checkboxes to make any object solid or controllable via `WASD` / Arrow Keys, with a direct `Speed` slider.

