# 2D Texture & Image System (`Renderer2D` & `SpriteComponent`)

The **2D Texture System** in Talos Engine allows any entity to display `.png` images with full alpha transparency, non-destructive scaling, and hardware color tinting, while preserving the engine's zero-overhead flat architecture and First-Level Inspector workflow.

---

## 1. Architectural Design & Layer Isolation

In many game engines, assigning an image to a 2D object requires creating a separate `Sprite2D` node, importing a texture asset through an external metadata pipeline, and linking resource GUIDs.

Talos Engine streamlines this across three decoupled layers:
1. **Layer 2 (`Renderer::Renderer2D`):** Owns the GPU texture cache (`s_textureCache`) and handles lazy disk loading, VRAM lifetime management, nearest-neighbor point filtering, and missing-texture fallback diagnostics.
2. **Layer 3 (`Scene::SpriteComponent`):** Stores a plain `std::string texturePath` alongside `ColorRGBA color`. No GPU pointers or external graphics headers leak into the Scene layer.
3. **Layer 5 (`Editor::Editor`):** Exposes one-click texture presets, a live file-path text input field, a `[Texture OK]` / `[Missing!]` status badge, and entity list sprite thumbnails directly on the First-Level Inspector.

---

## 2. Lazy GPU Texture Caching (`s_textureCache`)

Instead of forcing developers to write manual `LoadTexture` / `UnloadTexture` lifecycle code for every entity, `Renderer2D::DrawSprite` manages GPU textures automatically:

* **On-Demand First-Use Loading:** The first time an entity requests a texture path (e.g., `hero.png`), `Renderer2D` verifies the file on disk, decodes the `.png` image, uploads it to GPU VRAM, and stores the handle in `s_textureCache`.
* **O(1) Subsequent Draws:** On every subsequent frame—even if 500 entities share the same texture path—`Renderer2D` retrieves the cached GPU texture handle in $O(1)$ time with zero disk I/O.
* **Negative Lookup Protection (`s_missingTexturePaths`):** When a user is actively typing a filename in the Inspector text field, intermediate paths do not exist on disk. `Renderer2D` records missing strings in a lightweight set so the filesystem is never queried repeatedly on every frame for the same invalid path.
* **Automatic VRAM Cleanup:** When the engine closes, `Renderer2D::Shutdown()` iterates through `s_textureCache` and releases every GPU texture handle automatically.

---

## 3. Non-Destructive Scaling, Alpha Transparency & Hardware Tinting

When `Renderer2D::DrawSprite(texturePath, x, y, width, height, tint)` renders a valid texture:
1. **Full-Source Mapping:** The entire source image dimensions (`0, 0, tex.width, tex.height`) are mapped onto the entity's `TransformComponent` destination rectangle (`x, y, width, height`). Resizing an entity via the `Width` and `Height` sliders scales the rendered texture immediately without mutating the source image.
2. **Crisp Pixel Filtering:** Textures are configured with nearest-neighbor (`TEXTURE_FILTER_POINT`) sampling so pixel art and crisp 2D sprites never suffer from blurry bilinear interpolation when scaled up.
3. **Hardware Multiplicative Tinting:** The `SpriteComponent::color` property acts as a solid fill color when `texturePath` is empty, and automatically switches to a **GPU multiplicative tint** when a texture is active:
   * Setting `Img Tint` to **Pure White (`255, 255, 255, 255`)** renders the image's exact original colors.
   * Choosing any other swatch or adjusting the `R`, `G`, `B` sliders tints the sprite on the GPU at zero performance cost (ideal for hit flashes, team colors, or environmental lighting).

---

## 4. Visual Diagnostic Fallback (Missing Texture Checkerboard)

If an entity specifies a non-empty `texturePath` that cannot be found on disk, `Renderer2D::DrawSprite` renders a **2x2 magenta and dark-slate checkerboard pattern** inside the entity's bounding box, and the Inspector displays a red `[Missing!]` badge. This gives immediate visual feedback without crashing or halting the game loop.

---

## 5. Built-In Starter `.png` Assets

On startup, `Renderer2D::Init()` verifies whether the starter textures exist on disk and automatically generates five real, editable `.png` files if absent:
* **`hero.png` (`64x64`):** Stylized character sprite with visor, glowing cyan eyes, and transparent outer corners.
* **`crate.png` (`64x64`):** Wooden storage crate with plank seams, vertical brace, and corner bolts.
* **`brick.png` (`64x64`):** Staggered stone brick wall pattern with mortar lines.
* **`coin.png` (`64x64`):** Golden coin icon with a fully transparent alpha background (`alpha = 0`).
* **`sheet.png` (`256x64`):** 4-frame horizontal sprite sheet (`4 x 64x64` cells) featuring a cohesive Cyber Hero walk/pulse animation cycle for immediate testing of UV sub-rectangles and the 2D Frame Animator.

Developers can switch between `None`, `Hero`, `Crate`, `Brick`, `Coin`, and `Sheet` with a single click in the Inspector, edit those `.png` files in any image editor, or drop their own `.png` files into the project and type the path in the `Image:` box.
