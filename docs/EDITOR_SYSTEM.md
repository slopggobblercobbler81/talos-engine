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

## 4. The "First-Level Inspector" & On-Demand Component Attachment

To keep the Inspector clean and avoid showing inactive checkboxes for every possible capability on every object, newly created entities start with **only the fundamental `Transform 2D` component**. Optional components are attached on demand:

1. **Identity & Lifecycle (Always Visible):** Direct `Active` checkbox, live-editable `Name` and `Tag` text fields, and quick preset tag buttons.
2. **Transform 2D (Fundamental, Always Present):** Direct sliders for `Pos X`, `Pos Y`, `Width`, and `Height`.
3. **Attached Optional Components (Shown Only When Added):**
   - **Render Depth (`Sorting Layer` & `Z-Order`):** Automatically appears whenever a visual component (`Sprite 2D` or `Text Label`) is attached.
   - **Sprite 2D:** Texture selector, UV region, 2D frame-sheet animator, and **8 one-click preset color swatches** + direct `R`, `G`, `B` sliders.
   - **Text Label:** Live keyboard text input box + `Font Size` slider + `Offset Y` slider + **8 one-click preset color swatches** + direct `R`, `G`, `B` sliders.
   - **Collider 2D:** Box / Circle shape selector, custom radius, bounciness (`Restitution`), and `Dynamic` / `Static` / `Trigger` mode toggles.
   - **Movement 2D:** `Player Controlled (WASD)` toggle, `Speed` / `Velocity`, `Mass`, and `Gravity (%)`.
   - **Camera 2D:** Primary toggle, `Follow` / `Fixed` framing, target entity, damping speed, zoom, and framing offsets.
   - Each attached optional component header features a right-aligned **`[X]`** button to remove the component (`enabled = false`) with full Undo/Redo support.
4. **`+ Add Component` Dropdown Button:** Located at the bottom of the Inspector, unfolding a list of unattached components (`Sprite 2D`, `Text Label`, `Collider 2D`, `Movement 2D`, `Camera 2D`) that can be added to the selected entity with a single click.

---

## 5. Top Toolbar & Scene Serialization Integration

In **Edit Mode**, the Top Toolbar provides one-click persistence controls on the right side of the viewport bar:
* **`Save Scene`:** Invokes `Scene::SceneSerializer::Save` to write the active scene to disk in the human-readable `.talos` format (`scenes/main.talos`) and displays a temporary confirmation badge (`Saved -> scenes/main.talos`) in the toolbar.
* **`Load Scene`:** Invokes `Scene::SceneSerializer::Load` to hot-reload the saved `.talos` scene from disk into the active editor session without restarting the executable.
