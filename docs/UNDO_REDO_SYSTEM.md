# Universal Undo / Redo System

## Overview

The **Undo / Redo System** allows creators to reverse (`Ctrl + Z`) and reapply (`Ctrl + Y` or `Ctrl + Shift + Z`) any modification made in the **Talos Engine** Editor while in Edit Mode.

Instead of restricting Undo/Redo to a narrow set of operations, the system captures **every** authoring action across the Editor:
1. **Viewport Dragging:** Moving entities across the 2D canvas with the mouse.
2. **Text Editing:** Typing characters or pressing `Backspace` inside any Inspector text box (`Name:`, `Text:`, `Image:`).
3. **Slider Dragging:** Adjusting numerical values (`Pos X`, `Pos Y`, `Width`, `Height`, `Rotation`, `Font Size`, `Mass`, `Bounce`).
4. **Discrete Toggles & Presets:** Toggling checkboxes (`Sprite`, `Text`, `Physics`, `Use Gravity`, `Static Body`), picking color swatches, and selecting quick texture presets.
5. **Hierarchy & Scene Operations:** Creating new entities (`+ New Entity`), duplicating the selected entity (`Ctrl+D` or `Duplicate` toolbar button), deleting entities (`Delete` button or `Del` key), and loading scenes from disk (`Load Scene`).

---

## Architectural Design

### 1. Value-Type Scene Snapshots (`EditorHistoryState`)

Because Talos Engine enforces a **flat entity composition model** (`Entity` is a pure value-type struct containing POD and `std::string` fields with zero heap-allocated pointers or inheritance trees), capturing the entire state of a scene is fast and deterministic.

Each entry in the history stack is stored as an `EditorHistoryState`:
- `entities`: A complete `std::vector<Entity>` snapshot of the scene's entities and all their component data.
- `selectedEntityId`: The `EntityID` that was selected in the Editor when the snapshot was recorded, restoring the user's selection context alongside the scene data.

### 2. Dual History Stacks (`m_undoStack` & `m_redoStack`)

- **`m_undoStack`**: Stores up to `MAX_HISTORY_STEPS` (128) chronological snapshots prior to modifications.
- **`m_redoStack`**: Stores snapshots popped by `PerformUndo` so they can be restored via `PerformRedo`. Any new user edit clears `m_redoStack` to maintain linear history consistency.

---

## Discrete vs. Continuous Edit Coalescing

A key challenge in immediate-mode editors is distinguishing between **single-frame discrete actions** and **multi-frame continuous drags**:

### Discrete Actions (`PushUndoState`)
When the user performs an instantaneous action—such as typing a character into a text field, pressing `Backspace`, toggling a checkbox, clicking a color swatch, or spawning/deleting an entity—the Editor calls `PushUndoState(scene)` **immediately before** mutating the data.
- For text input, recording a snapshot before each character insertion or deletion (with OS key-repeat enabled for `Backspace` and `Ctrl + Z`) allows the user to undo/redo typing character-by-character, even while the text box remains focused.

### Continuous Drags (`BeginContinuousEdit` & `CommitContinuousEdit`)
When the user drags an entity in the 2D viewport or drags a numerical slider in the Inspector, the value changes every frame for dozens of frames. Pushing a snapshot on every frame would flood the history stack with intermediate values.

To coalesce an entire drag into **one single Undo step**:
1. **On Mouse Press (`BeginContinuousEdit`)**: When a viewport drag or slider drag begins, the Editor captures a temporary baseline snapshot (`m_dragStartState`) before any frame mutation occurs.
2. **During Mouse Hold**: The entity's position or component property updates live in real time without touching the history stacks.
3. **On Mouse Release (`CommitContinuousEdit`)**: When the mouse button is released, the Editor compares the scene's final entity list against `m_dragStartState.entities` using `AreEntityListsEqual`:
   - If any property changed during the drag, `m_dragStartState` is pushed onto `m_undoStack` as a single entry and `m_redoStack` is cleared.
   - If nothing changed (for example, clicking an entity in the viewport purely to select it without moving it), the baseline snapshot is discarded so no no-op entries pollute the Undo stack.

---

## Keyboard Shortcut Priority & Text Box Interception

During `Editor::Update`, keyboard shortcuts (`Ctrl + Z`, `Ctrl + Y`, `Ctrl + Shift + Z`) are evaluated **before** text input polling:
- Holding `Ctrl` and pressing `Z` or `Y` triggers `PerformUndo` or `PerformRedo` (supporting OS key-repeat when held down) and drains the character queue (`Input::GetCharPressed()`).
- This guarantees that pressing `Ctrl + Z` while typing inside an active Inspector text field immediately undoes the last typed character instead of inserting control characters into the string.
- Entering Play Mode (`Space`) commits any active drag and suspends Undo/Redo shortcuts until the Editor returns to Edit Mode.
