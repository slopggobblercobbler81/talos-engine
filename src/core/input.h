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
        Up,
        Down,
        Left,
        Right,
        Space,
        Escape
    };

    // =========================================================================
    // Input Subsystem (Layer 1: Core & Platform)
    // =========================================================================
    // Provides a unified, stateless query interface for hardware input devices.
    class Input {
    public:
        // Returns true every frame while the physical key is held down.
        // Best suited for continuous actions such as character movement.
        static bool IsKeyDown(Key key);

        // Returns true only on the single frame the key transitions from up to down.
        // Best suited for discrete actions such as jumping, shooting, or toggling UI.
        static bool IsKeyPressed(Key key);
    };

}
