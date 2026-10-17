#pragma once

namespace Talos::Core {

    // =========================================================================
    // Engine Key Codes (Layer 1: Core & Platform)
    // =========================================================================
    // Defines engine-owned key identifiers using a strongly-typed C++ enum class.
    // Using our own enum prevents third-party key macros from leaking into
    // gameplay, scene, or editor code.
    enum class Key {
        W,
        A,
        S,
        D,
        Z,
        Y,
        Up,
        Down,
        Left,
        Right,
        Space,
        Escape,
        Backspace,
        Delete,
        Enter,
        LeftControl,
        RightControl,
        LeftShift,
        RightShift
    };

    // =========================================================================
    // Engine Mouse Buttons (Layer 1: Core & Platform)
    // =========================================================================
    enum class MouseButton {
        Left,
        Right,
        Middle
    };

    // =========================================================================
    // Input Subsystem (Layer 1: Core & Platform)
    // =========================================================================
    // Provides a unified, stateless query interface for keyboard and mouse devices.
    class Input {
    public:
        // Returns true every frame while the physical key is held down.
        // Best suited for continuous actions such as character movement or modifier keys (Ctrl/Shift).
        static bool IsKeyDown(Key key);

        // Returns true only on the single frame the key transitions from up to down
        // (or on OS key-repeat events when held down).
        // Best suited for discrete actions such as jumping, shortcuts (Ctrl+Z), or toggling UI.
        static bool IsKeyPressed(Key key);

        // Polls the next typed character from the OS keyboard queue (ASCII/Unicode).
        // Returns 0 when no more characters were typed during the current frame.
        // Used by Editor text input fields for live typing.
        static int GetCharPressed();

        // Returns the current horizontal and vertical cursor coordinates in window pixels.
        static float GetMouseX();
        static float GetMouseY();

        // Returns true every frame while the specified mouse button is held down.
        // Used for dragging entities in the viewport or sliding Inspector bars.
        static bool IsMouseButtonDown(MouseButton button);

        // Returns true only on the single frame the mouse button is initially clicked.
        // Used for clicking UI buttons or selecting entities in the viewport.
        static bool IsMouseButtonPressed(MouseButton button);

        // Returns true only on the single frame the mouse button is released.
        // Used to end drag-and-drop operations cleanly and commit Undo snapshots.
        static bool IsMouseButtonReleased(MouseButton button);
    };

}
