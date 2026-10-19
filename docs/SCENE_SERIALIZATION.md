# Scene Serialization System (`SceneSerializer` & `.talos` Format)

The **Scene Serialization System** sits inside the Scene & Entity Layer (Layer 3) and is responsible for persisting the world state to disk and reconstructing it on startup or on demand.

---

## 1. Solving Version Control & Merge Conflict Friction

A frequent complaint in mainstream engines is how scene files are stored on disk:
* **Heavyweight YAML / GUID Noise:** Minor edits in complex YAML scene formats often rewrite dozens of lines or depend on opaque internal GUID references.
* **Fragile Sub-Resource Indexing:** Deeply nested scene formats that rely on sequential sub-resource IDs and hierarchy paths break easily when two developers edit the same scene or when objects are reordered.

Talos Engine solves this with the **`.talos` Flat Section Format**:
* Every entity is serialized as an independent, self-contained `[Entity]` block.
* Properties are always written in a **strictly deterministic order** with two-decimal floating-point formatting (`0.00`).
* Moving an entity in the Editor changes **only** its `transform.x` and `transform.y` lines in Git diffs, leaving the rest of the file completely untouched.

---

## 2. Structure of a `.talos` Scene File

Each `[Entity]` block records the entity's identity and the state of its flat components using simple `key = value` pairs:

```ini
# Talos Engine Scene File (v1)
# Human-readable, deterministic flat entity serialization

[Entity]
id = 2
name = Player
tag = Player
active = 1
transform.x = 354.00
transform.y = 323.00
transform.width = 72.00
transform.height = 72.00
sprite.enabled = 1
sprite.texturePath = examples/assets/textures/sheet.png
sprite.color = 255,255,255,255
sprite.sortingLayer = 2
sprite.zOrder = 10
sprite.sourceX = 0.00
sprite.sourceY = 0.00
sprite.sourceW = 64.00
sprite.sourceH = 64.00
sprite.flipX = 0
sprite.flipY = 0
sprite.frameCount = 4
sprite.currentFrame = 0
sprite.fps = 8.00
sprite.loop = 1
sprite.isPlaying = 1
text.enabled = 1
text.content = Player
text.fontSize = 18
text.color = 240,240,240,255
text.offsetX = 0.00
text.offsetY = -24.00
collider.enabled = 1
collider.shape = 0
collider.radius = 0.00
collider.restitution = 0.00
collider.isSolid = 1
collider.isStatic = 0
collider.isTrigger = 0
movement.enabled = 1
movement.playerControlled = 1
movement.speed = 320.00
movement.mass = 2.00
movement.gravityScale = 0.00
movement.velocityX = 0.00
movement.velocityY = 0.00
camera.enabled = 1
camera.isPrimary = 1
camera.followTarget = 1
camera.smoothFollow = 1
camera.targetName = Player
camera.followSpeed = 8.00
camera.zoom = 1.00
camera.offsetX = 0.00
camera.offsetY = 0.00
audio.enabled = 1
audio.audioPath = examples/assets/audio/coin.wav
audio.isMusic = 0
audio.playOnStart = 1
audio.loop = 0
audio.playOnCollision = 0
audio.volume = 1.00
audio.pitch = 1.00
audio.pan = 0.50
```

---

## 3. Serialization & Deserialization Pipeline

### Saving (`SceneSerializer::Save`)
1. **Automatic Directory Provisioning:** Uses C++17 `std::filesystem::create_directories` to ensure parent folders exist before opening the output stream.
2. **Sequential Component Dump:** Iterates over `Scene::GetEntities()` and writes each entity's fields in fixed order.

### Loading (`SceneSerializer::Load`)
1. **Clean Slate Reset:** Calls `Scene::Clear()` to empty existing entities and reset the ID counter.
2. **Single-Pass Line Parser:** Reads the `.talos` file line by line, stripping whitespace and ignoring `#` comments.
3. **ID Counter Synchronization (`Scene::AddEntity`):** As each parsed `[Entity]` block is committed to the `Scene`, `Scene::AddEntity` ensures that the internal `m_nextId` counter always stays strictly greater than the highest loaded `EntityID` (`m_nextId = max(m_nextId, entity.id + 1)`). This guarantees that creating new entities in the Editor after loading a file never produces duplicate IDs.

---

## 4. Unified Project File (`project.talos` & `ProjectSerializer`)

Alongside individual scene files, `Scene::ProjectSerializer` (`engine/scene/project_serializer.h`) manages the unified project manifest (`project.talos`). It persists three deterministic flat sections:

```ini
# Talos Engine Unified Project File (v1)
# Deterministic flat project configuration, initial scene, audio mixer, and input map

[Project]
name = Talos Starter Project
version = 0.1.0
windowWidth = 1280
windowHeight = 720
targetFps = 60
fullscreen = 0
resizable = 1
startScene = examples/scenes/main.talos

[Audio]
masterVolume = 1.00
sfxVolume = 1.00
musicVolume = 0.85

[InputMap]
action.move_left = A,Left
action.move_right = D,Right
action.move_up = W,Up
action.move_down = S,Down
action.jump = Space,W,Up
action.attack = J,Enter
action.interact = E,F
action.pause = Escape,P
```

### High-Level Project Operations
- **`ProjectSerializer::CreateNewProject(projectFilePath, config, scene, outSceneFilePath)`:** Sanitizes the configuration, populates a starter scene (`Player`, `Ground`, and `Banner`), saves the initial `.talos` scene relative to `project.talos`, applies audio/input/window settings to the live engine, and writes `project.talos`.
- **`ProjectSerializer::OpenProject(projectFilePath, outConfig, scene, outSceneFilePath)`:** Loads `project.talos`, applies `[Project]`, `[Audio]`, and `[InputMap]` settings to `Window`, `AudioSystem`, and `InputMap`, resolves `startScene` relative to the project root, and loads the initial scene into `Scene` (or creates a starter scene if the file does not exist yet).
- **`ProjectSerializer::SaveProject(projectFilePath, config, scene, activeSceneFilePath)`:** Captures live audio bus volumes and input bindings from the engine, writes `project.talos`, and saves the active `Scene` to disk.

---

## 5. Reusable Entity Templates (`.prefab` Files)

`SceneSerializer` also supports saving and instantiating individual configured entities as standalone **`.prefab`** files (`examples/prefabs/*.prefab`). Both `.talos` scenes and `.prefab` templates share the exact same deterministic property writer and parser (`WriteEntityProperties` / `ParseEntityProperty`), except a `.prefab` file omits the scene-specific `id` line and uses a `[Prefab]` header block:

```ini
# Talos Engine Prefab File (v1)
# Reusable flat entity template (.prefab)

[Prefab]
name = Gold Coin
tag = Coin
active = 1
transform.x = 480.00
transform.y = 300.00
transform.width = 40.00
transform.height = 40.00
...
```

### Prefab API (`SceneSerializer`)
- **`SceneSerializer::SavePrefab(entity, filePath)`:** Automatically creates parent directories and writes all 7 component groups (`Transform`, `Sprite`, `Text`, `Collider`, `Movement`, `Camera`, `Audio`) of `entity` into a deterministic `.prefab` file.
- **`SceneSerializer::LoadPrefab(filePath, outEntity)`:** Parses a `[Prefab]` block into `outEntity` while resetting `id = 0` and clearing transient runtime state (`frameTimer`, `currentX/Y`, `runtimeInitialized`, `runtimeStarted`).
- **`SceneSerializer::InstantiatePrefab(scene, filePath, overrideX, overrideY, useOverridePosition)`:** Loads the `.prefab` template, resolves a conflict-free sequential name in `scene` (e.g., `"Gold Coin"`, `"Gold Coin_2"`, `"Gold Coin_3"`), optionally overrides the spawn coordinates `(transform.x, transform.y)`, assigns a fresh unique `EntityID`, and appends the instance to `scene`.
- **`SceneSerializer::BuildPrefabPath(entityName, directory)`:** Normalizes an entity name into a clean lowercase snake_case `.prefab` path (e.g., `"Gold Coin"` $\rightarrow$ `"examples/prefabs/gold_coin.prefab"`).

