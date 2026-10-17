# Core & Platform System (`Window` & `Input`)

The **Core & Platform System** forms the lowest layer of Talos Engine. Its responsibility is to manage the operating system window lifecycle, frame timing synchronization, and hardware input polling while completely hiding the underlying platform backend from the rest of the engine.

---

## 1. Window & Frame Timing (`Window`)

### Purpose
Every game engine requires an OS surface to draw into and a reliable clock to measure how much time passes between frames. The `Window` subsystem centralizes these responsibilities behind a clean static interface.

### How It Works
1. **Initialization & Frame Pacing (`Window::Init`):**
   Creates the native window and OpenGL context and sets a default target frame rate (60 FPS). Locking the frame rate ensures that the engine yields CPU time between frames rather than spinning at 100% CPU utilization.
2. **Termination Polling (`Window::ShouldClose`):**
   Checks OS events each frame to detect if the user clicked the close button or triggered a system exit shortcut.
3. **Frame-Rate Independence (`Window::GetDeltaTime`):**
   Returns `delta`—the exact duration in seconds of the previously completed frame (for example, `0.0166s` at 60 FPS).
   * **Why `delta` matters:** If an object moves by `5 pixels` per frame without `delta`, it will move twice as fast on a 120 Hz monitor as on a 60 Hz monitor. By multiplying speed (in pixels per second) by `delta`, movement speed remains identical across all hardware:
     $$\text{displacement} = \text{speed} \times \Delta t$$

---

## 2. Hardware Input Isolation (`Input`)

### Purpose
Directly referencing third-party key macros throughout an engine tightly couples gameplay and scene logic to a specific library. The `Input` subsystem prevents this by defining an engine-native `Key` enumeration and a dedicated translation boundary.

### How It Works
1. **Strongly-Typed Key Enumeration (`enum class Key`):**
   Defines engine-owned key identifiers (`Key::W`, `Key::A`, `Key::Space`, etc.). Using a C++ `enum class` prevents implicit integer conversions and naming collisions.
2. **Private Translation Table (`ToBackendKey`):**
   Inside the internal implementation, a `static` helper function translates `Talos::Core::Key` values into the integer key codes expected by the underlying platform backend. Because this function is marked `static` inside the implementation unit, it is completely invisible outside the module.
3. **Continuous vs. Discrete Queries:**
   * `Input::IsKeyDown(Key)`: Returns `true` on every frame the physical key remains held down. Used for continuous states such as walking or flying.
   * `Input::IsKeyPressed(Key)`: Returns `true` only on the single frame the key transitions from released to pressed. Used for one-shot actions such as jumping, firing a projectile, or toggling an editor mode.

