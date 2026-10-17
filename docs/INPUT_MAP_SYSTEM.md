# Configurable Input Map System (`Talos::Core::InputMap`)

## Overview

The **Talos Engine Input Map** (`Talos::Core::InputMap`, defined in `core/input.h` and `core/input.cpp`) provides a **Godot-style Logical Action Mapping System**. It decouples game logic and future scripting code from raw physical keyboard keys by allowing developers to define named actions (such as `"jump"`, `"attack"`, `"interact"`, or `"move_left"`), bind one or more physical keys to each action in the Visual Editor, and query those actions by name at runtime.

---

## Pre-Populated Default Video Game Actions

Out of the box (and whenever **Reset Defaults** is clicked), the Input Map comes pre-populated with **15 of the most commonly used actions in 2D and 3D video games**:

| Action Name (`Code ID`) | Default Bound Keys | Typical Video Game Usage |
| :--- | :--- | :--- |
| `move_left` | `A`, `Left Arrow` | Horizontal movement left |
| `move_right` | `D`, `Right Arrow` | Horizontal movement right |
| `move_up` | `W`, `Up Arrow` | Vertical movement up / climb |
| `move_down` | `S`, `Down Arrow` | Vertical movement down / drop |
| `jump` | `Space` | Character jump |
| `attack` | `J`, `Z` | Primary attack / fire |
| `interact` | `E`, `F` | Interact with NPCs, doors, chests, or levers |
| `sprint` | `Left Shift` | Run / sprint modifier |
| `crouch` | `Left Ctrl`, `C` | Crouch / stealth stance |
| `dash` | `K`, `X` | Evade / dodge roll / air dash |
| `reload` | `R` | Reload weapon or ability |
| `inventory` | `Tab`, `I` | Open player inventory / equipment screen |
| `pause` | `Escape`, `P` | Pause or unpause the game |
| `ui_accept` | `Enter`, `Space` | Confirm menu selection or advance dialogue |
| `ui_cancel` | `Escape`, `Backspace` | Cancel menu selection or go back |

---

## Interactive Input Map Editor UI

In **Edit Mode**, clicking the **`Input Map`** button in the Top Toolbar opens the centered **Input Map Configuration Modal**:

1. **Create New Actions:**
   - Type any custom action identifier into the **`Action:`** input box at the top (spaces are automatically converted to underscores `_` for clean code identifiers) and click **`+ Add Action`** or press **`Enter`**.
2. **Bind Physical Keys (`+ Bind Key`):**
   - Click **`+ Bind Key`** on any action row to enter **Key Listening Mode** (`Press Key...`).
   - Press any key on the physical keyboard (`A-Z`, `0-9`, Arrow keys, `Space`, `Enter`, `Escape`, `Tab`, `Backspace`, `Delete`, `Left/Right Shift`, `Left/Right Ctrl`, `Left/Right Alt`, or `F1-F12`) to bind it to that action.
   - Right-click anywhere to cancel Key Listening Mode without binding a key (allowing `Escape` itself to be bound to actions like `pause` or `ui_cancel`).
3. **Unbind Individual Keys:**
   - Each bound key is displayed as an interactive chip (`[ Space  x ]`). Clicking any key chip immediately unbinds that key from the action.
4. **Live Hardware State Testing (`[ON]` / `[OFF]`):**
   - Each action row displays a live **`ON` / `OFF`** state indicator that lights up in real time whenever any of its assigned physical keys are held down.
5. **Delete Actions & Reset Defaults:**
   - Click **`Del`** on any row to remove an action, or click **`Reset Defaults`** in the modal header to restore the 15 standard video game actions.

---

## Querying Actions from Engine or Gameplay Code

When gameplay systems or scripts need to read player input, they can query actions by name through `Talos::Core::InputMap` (or directly through the forwarded methods on `Talos::Core::Input`):

```cpp
#include "core/input.h"

using namespace Talos::Core;

void UpdatePlayerExample(float delta) {
    // 1. Continuous state (true every frame while ANY bound key is held)
    if (InputMap::IsActionDown("sprint")) {
        // Apply sprint speed multiplier
    }

    // 2. Single-frame press event (true ONLY on the frame a bound key is pressed)
    if (InputMap::IsActionPressed("jump")) {
        // Trigger jump impulse
    }

    // 3. Single-frame release event (true ONLY on the frame a bound key is released)
    if (InputMap::IsActionReleased("attack")) {
        // Release charged attack
    }

    // 4. Composite 1D axis helper in [-1.0f, +1.0f]
    float moveX = InputMap::GetAxis("move_left", "move_right");
    float moveY = InputMap::GetAxis("move_up", "move_down");
}
```

---

## Plain-Text Persistence (`examples/scenes/input_map.talos`)

All changes made in the Input Map modal are automatically saved to `examples/scenes/input_map.talos` in a clean, human-readable key-value format that is version-control friendly and validated on load with full Debug Console diagnostics (`[file:line]` warnings on malformed lines or unknown key tokens).
