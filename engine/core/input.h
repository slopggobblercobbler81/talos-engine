// Engine key/mouse codes and the Input polling API (keyboard, mouse, typed characters)
#pragma once
#include <string>
#include "core/input_map.h"

namespace Talos::Core {

    // =========================================================================
    // Engine Key Codes (Layer 1: Core & Platform)
    // =========================================================================
    // Defines engine-owned key identifiers using a strongly-typed C++ enum class.
    // Using our own enum prevents third-party key macros from leaking into
    // gameplay, scene, scripting, or editor code.
    enum class Key {
        Unknown = 0,

        // Alphabet keys (A - Z)
        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

        // Top-row Number keys (0 - 9)
        D0, D1, D2, D3, D4, D5, D6, D7, D8, D9,

        // Directional Arrow keys
        Up,
        Down,
        Left,
        Right,

        // Common Action & Navigation keys
        Space,
        Enter,
        Escape,
        Tab,
        Backspace,
        Delete,

        // Modifier keys
        LeftShift,
        RightShift,
        LeftControl,
        RightControl,
        LeftAlt,
        RightAlt,

        // Function keys (F1 - F12)
        F1, F2, F3, F4, F5, F6,
        F7, F8, F9, F10, F11, F12
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
    // Provides a unified query interface for keyboard, mouse, and logical Input Map actions.
    class Input {
    public:
        // Returns true every frame while the physical key is held down.
        static bool IsKeyDown(Key key);

        // Returns true only on the single frame the key transitions from up to down
        // (or on OS key-repeat events when held down).
        static bool IsKeyPressed(Key key);

        // Returns true only on the single frame the key transitions from down to up
        // (without OS key-repeat events).
        static bool IsKeyPressedOnce(Key key);

        // Returns true only on the single frame the physical key is released.
        static bool IsKeyReleased(Key key);

        // Scans all supported keyboard keys and outputs the first key pressed on this frame.
        // Used by the Editor's Input Map modal when listening for a new key binding.
        static bool PollAnyKeyPressed(Key& outKey);

        // Returns a human-readable display label for a key (e.g., "Space", "Left Shift", "Up Arrow").
        static const char* GetKeyName(Key key);

        // Serialization helpers converting between Key enum values and string tokens.
        static const char* GetKeyToken(Key key);
        static bool TryParseKeyToken(const std::string& token, Key& outKey);

        // =====================================================================
        // Logical Action Queries (Forwarded to InputMap for Convenience)
        // =====================================================================
        static bool IsActionDown(const std::string& actionName);
        static bool IsActionPressed(const std::string& actionName);
        static bool IsActionReleased(const std::string& actionName);
        static float GetAxis(const std::string& negativeAction, const std::string& positiveAction);

        // Polls the next typed character from the OS keyboard queue (ASCII/Unicode).
        // Returns 0 when no more characters were typed during the current frame.
        static int GetCharPressed();

        // Returns the current horizontal and vertical cursor coordinates in window pixels.
        static float GetMouseX();
        static float GetMouseY();

        // Returns vertical mouse wheel scroll delta for the current frame.
        static float GetMouseWheelMove();

        // Mouse button state queries.
        static bool IsMouseButtonDown(MouseButton button);
        static bool IsMouseButtonPressed(MouseButton button);
        static bool IsMouseButtonReleased(MouseButton button);
    };

}
