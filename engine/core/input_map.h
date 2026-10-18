// Logical input actions (InputMap): key bindings and .talos persistence
#pragma once
#include <string>
#include <vector>

namespace Talos::Core {

    // Forward declaration of engine key codes (defined in core/input.h)
    enum class Key;

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

}
