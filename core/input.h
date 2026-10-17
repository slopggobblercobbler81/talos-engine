#pragma once
#include <string>
#include <vector>

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
    // Logical Input Action (Layer 1: Core & Platform)
    // =========================================================================
    // Maps a named gameplay action (e.g., "move_left", "jump", "attack", "interact")
    // to one or more physical keyboard keys, inspired by Godot's Input Map system.
    struct InputAction {
        std::string name;
        std::vector<Key> keys;
    };

    // =========================================================================
    // Configurable Input Map (Layer 1: Core & Platform)
    // =========================================================================
    // Stores all user-defined and built-in logical gameplay actions and their
    // assigned key bindings. Allows gameplay code and future scripts to query
    // actions by name (e.g., InputMap::IsActionDown("jump")) decoupled from
    // physical hardware keys.
    class InputMap {
    public:
        // Populates the Input Map with the most commonly used video game actions
        // ("move_left", "move_right", "move_up", "move_down", "jump", "attack",
        // "interact", "sprint", "crouch", "dash", "reload", "inventory", "pause",
        // "ui_accept", "ui_cancel").
        static void InitDefaults();

        // Read-only and mutable access to the ordered list of configured actions.
        static const std::vector<InputAction>& GetActions();
        static std::vector<InputAction>& GetActionsMutable();

        // Looks up an action by its exact name (returns nullptr if not found).
        static const InputAction* FindAction(const std::string& actionName);
        static bool HasAction(const std::string& actionName);

        // Creates a new action with no initial keys (returns false if empty or duplicate).
        static bool AddAction(const std::string& actionName);

        // Deletes an action by name (returns true if found and removed).
        static bool RemoveAction(const std::string& actionName);

        // Assigns a key to an existing action (ignores duplicate keys on the same action).
        static bool BindKey(const std::string& actionName, Key key);

        // Removes a specific key from an existing action.
        static bool UnbindKey(const std::string& actionName, Key key);

        // Removes all keys bound to an action without deleting the action itself.
        static bool ClearActionKeys(const std::string& actionName);

        // =====================================================================
        // Runtime Action Queries (For Gameplay Code & Future Scripting Engine)
        // =====================================================================

        // Returns true every frame while ANY key assigned to 'actionName' is held down.
        static bool IsActionDown(const std::string& actionName);

        // Returns true only on the single frame ANY key assigned to 'actionName' is pressed.
        static bool IsActionPressed(const std::string& actionName);

        // Returns true only on the single frame ANY key assigned to 'actionName' is released.
        static bool IsActionReleased(const std::string& actionName);

        // Computes a 1D directional axis in [-1.0f, +1.0f] from two opposing actions
        // (e.g., InputMap::GetAxis("move_left", "move_right")).
        static float GetAxis(const std::string& negativeAction, const std::string& positiveAction);

        // =====================================================================
        // Persistence (.talos Input Map File)
        // =====================================================================
        static bool Save(const std::string& filePath = "examples/scenes/input_map.talos");
        static bool Load(const std::string& filePath = "examples/scenes/input_map.talos");
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
