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
* **Snap to Grid (`[G]` / Viewport HUD Toggle):** Toggling **`Snap: ON [G]`** in the bottom-left Viewport HUD bar (or pressing the `G` key in Edit Mode) snaps dragged entity coordinates `(x, y)` to the nearest grid multiple (`std::round(coord / gridStep) * gridStep`) with full Undo/Redo coalescing. Clicking the step badge (`40px`) cycles through standard 2D pixel grid steps (`8px`, `16px`, `32px`, `40px`, `64px`) and updates the visual infinite grid in real time.

---

## 4. Scene Hierarchy Organization: Reordering, Search & Tag Filtering (`SceneTreeDock`)

The left-hand **Scene Hierarchy Dock** (`editor/docks/scene_tree_dock.h`) gives immediate control over entity organization, draw-order tie-breaking, and filtering in large scenes:
* **Live Search Box (`Find:`):** Filters the hierarchy in real time by case-insensitive substring match against both `entity.name` and `entity.tag` (`Scene::MatchesEntityFilter`). Supports explicit prefixes (`tag:enemy`, `#coin`, `name:hero`) and a one-click **`[X]`** button to clear active filters.
* **Quick Tag Filter Bar (`Tag:`):** One-click filter chips (`All`, `Player`, `Enemy`, `Coin`, `Wall`) that combine with the text search box and display a live `shown / total` counter badge in the header when filtering is active.
* **Hierarchy Reordering (Buttons, Drag-and-Drop & Shortcuts):**
  - **Header & Inline Row `[^]` / `[v]` Buttons:** Move the selected entity up or down in the scene list (`Scene::MoveEntityUp` / `Scene::MoveEntityDown`) with discrete Undo/Redo snapshots.
  - **Mouse Drag-and-Drop Reordering:** Dragging an entity row vertically inside the unfiltered Scene Hierarchy displays an insertion indicator line and drops the entity at the target index (`Scene::MoveEntityToIndex`) on mouse release.
  - **Keyboard Shortcuts (`Alt + Up` / `Alt + Down`):** Moves the selected entity up or down in Edit Mode with Undo/Redo support.
* **Visual Row Badges:** Each row displays a `16x16` sprite texture preview (or color swatch), a `[Tag]` badge, component capability badges (`[CAM]`, `[TRG]`, `[BOX]`/`[CIR]`), and mouse-wheel scrolling clipped cleanly above the bottom dock.

---

## 5. The "First-Level Inspector" & On-Demand Component Attachment

To keep the Inspector clean and avoid showing inactive checkboxes for every possible capability on every object, newly created entities start with **only the fundamental `Transform 2D` component**. Optional components are attached on demand:

1. **Identity & Lifecycle (Always Visible):** Direct `Active` checkbox, live-editable `Name` and `Tag` text fields, quick preset tag buttons, and a one-click **`Save Entity as Prefab (.prefab)`** button that exports the configured entity to `examples/prefabs/<name>.prefab` and focuses it in the Asset Browser.
2. **Transform 2D (Fundamental, Always Present):** Direct sliders for `Pos X`, `Pos Y`, `Width`, and `Height`.
3. **Attached Optional Components (Shown Only When Added):**
   - **Render Depth (`Sorting Layer` & `Z-Order`):** Automatically appears whenever a visual component (`Sprite 2D` or `Text Label`) is attached.
   - **Sprite 2D:** Texture selector, UV region, 2D frame-sheet animator, and **8 one-click preset color swatches** + direct `R`, `G`, `B` sliders.
   - **Text Label:** Live keyboard text input box + `Font Size` slider + `Offset Y` slider + **8 one-click preset color swatches** + direct `R`, `G`, `B` sliders.
   - **Collider 2D:** Box / Circle shape selector, custom radius, bounciness (`Restitution`), and `Dynamic` / `Static` / `Trigger` mode toggles.
   - **Movement 2D:** `Player Controlled (WASD)` toggle, `Speed` / `Velocity`, `Mass`, and `Gravity (%)`.
   - **Camera 2D:** Primary toggle, `Follow` / `Fixed` framing, target entity, damping speed, zoom, and framing offsets.
   - **Audio Source:** `SFX` (RAM) / `Music` (Stream) mode toggle, live `[Play]` / `[Stop]` preview buttons, editable `Clip` path, starter preset buttons (`Coin`, `Jump`, `Hit`, `Music`, `None`), `Play On Start` / `Loop` / `Play On Collision` toggles, and `Volume (%)`, `Pitch (%)`, and `Pan (L/R)` sliders.
   - Each attached optional component header features a right-aligned **`[X]`** button to remove the component (`enabled = false`) with full Undo/Redo support.
4. **`+ Add Component` Dropdown Button:** Located at the bottom of the Inspector, unfolding a list of unattached components (`Sprite 2D`, `Text Label`, `Collider 2D`, `Movement 2D`, `Camera 2D`, `Audio Source`) that can be added to the selected entity with a single click.

---

## 5. Top Toolbar, Project Workflow & Scene Serialization Integration

In **Edit Mode**, the Top Toolbar (`EditorToolbar`) and Project Modal (`ProjectDialog`) provide unified project and scene persistence controls:
* **`Project` Button & `ProjectDialog` (`editor/dialogs/project_dialog.h`):** Opens the **Project Settings & Unified Workflow (`project.talos`)** modal window:
  - **Workflow Bar (`New Project`, `Open Project`, `Save Project`):** Creates a fresh `project.talos` and starter scene, loads an existing `project.talos` (restoring window title, target FPS, audio bus volumes, input action bindings, and initial scene while resetting Undo/Redo history), or saves both `project.talos` and the active scene.
  - **General & Window Configuration:** Live-editable `Project File`, `Game Title`, and `Version` text inputs, `Width` / `Height` sliders, one-click resolution presets (`1280x720 HD`, `1600x900 HD+`, `1920x1080 FHD`, `960x540 Pixelx2`), `Target FPS` slider (`30` / `60` / `120` / `144` presets), and `Resizable` / `Fullscreen` flags.
  - **Initial Scene (`startScene`):** Editable relative path with quick buttons to **Use Active Editor Scene**, **Load Start Scene Now**, or **Save Active as Start Scene**.
  - **Subsystem Shortcuts:** Displays a live summary of `[Audio]` bus volumes and `[InputMap]` action count with one-click shortcuts to open the **Audio Mixer** or **Input Map** modals.
* **`+ New Entity`, `Duplicate` (`Ctrl+D`), & `Delete` (`Delete`):** Creates a new entity at the viewport center, immediately clones the selected entity (`Scene::DuplicateEntity`) with a unique sequential name and `(16, 16)` pixel offset while selecting the clone, or deletes the selected entity—all backed by discrete Undo/Redo snapshots.
* **`Ctrl+S` Shortcut:** Saves the unified `project.talos` file along with the active `.talsc` scene and displays a confirmation toast in the toolbar.
* **`Save Scene` / `Load Scene`:** Invokes `Scene::SceneSerializer::Save` (also syncing `project.talos` subsystem state) and `Scene::SceneSerializer::Load` to hot-reload the active `.talsc` scene without restarting the executable.

---

## 6. Bottom Dock Tab Switcher (`Debug Console` & `Asset Browser`)

The bottom dock (`ConsoleDock` + `AssetBrowserDock`) hosts a two-tab header bar that lets the user switch between real-time engine diagnostics and visual project resource management:
* **`Debug Console` Tab (`editor/docks/console_dock.h`):** Displays classified engine log messages (`[INFO]`, `[WARN]`, `[ERROR]`) with source file/line metadata, severity filter buttons, mouse-wheel scrolling, and a `Clear` button.
* **`Asset Browser` Tab (`editor/docks/asset_browser_dock.h`):** Provides a 3-column visual explorer for project files:
  - **Header Filter Bar:** Displays the active relative directory, category filter tabs (`All`, `Textures`, `Audio`, `Prefabs`, `Scenes`, `Scripts`), and a `Refresh` button.
  - **Left Column (Folder Shortcuts):** Quick navigation buttons (`examples/`, `textures`, `audio`, `prefabs`, `scenes`, and `[.. Up Folder]`).
  - **Center Column (Scrollable File List):** Lists subdirectories (`[DIR]`) and categorized project files (`[TEX]`, `[AUD]`, `[PFB]`, `[SCN]`, `[LUA]`, `[FILE]`) with human-readable byte sizes and mouse-wheel scrolling.
  - **Right Column (Live Preview & One-Click Assignment):**
    - **Textures (`.png`, `.bmp`, `.jpg`):** Renders a live GPU thumbnail, native pixel dimensions, and VRAM footprint, with one-click `Assign Sprite` (onto the selected entity) and `+ New Sprite` (spawns a new textured entity in the scene) actions backed by Undo/Redo.
    - **Audio (`.wav`, `.ogg`, `.mp3`):** Displays format, sample rate (`Hz`), channels, and duration (`s`), live `[Play SFX]`, `[Play Music]`, and `[Stop]` preview controls, and one-click `Assign as SFX` / `Assign as Music` buttons for the selected entity.
    - **Prefabs (`.prefab`):** Displays a live text preview of the `[Prefab]` template and a one-click **`+ Instantiate in Scene`** button (`SceneSerializer::InstantiatePrefab`) that spawns a configured copy in the active scene with Undo/Redo support, automatic selection, and `(+16, +16)` position nudging when another entity occupies the exact same spot. When no asset is selected, a quick **`Save '<name>' as Prefab`** button is also offered for the active entity.
    - **Scenes & Scripts (`.talsc`, `.lua`):** Displays a live text preview of the file contents and a one-click `Open Scene in Editor` button for `.talsc` scene files.


