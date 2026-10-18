#include "core/input_map.h"
#include "core/input.h"
#include "core/logger.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Talos::Core {

    namespace {

        std::string TrimString(const std::string& str) {
            const char* ws = " \t\r\n";
            size_t first = str.find_first_not_of(ws);
            if (first == std::string::npos) return "";
            size_t last = str.find_last_not_of(ws);
            return str.substr(first, last - first + 1);
        }

        // Global storage for the active Input Map actions.
        std::vector<InputAction> g_actions;
        bool g_initialized = false;

        void EnsureInitialized() {
            if (!g_initialized) {
                InputMap::InitDefaults();
            }
        }

        InputAction* FindActionMutable(const std::string& actionName) {
            EnsureInitialized();
            for (auto& action : g_actions) {
                if (action.name == actionName) {
                    return &action;
                }
            }
            return nullptr;
        }

    } // anonymous namespace

    void InputMap::InitDefaults() {
        g_initialized = true;
        g_actions.clear();

        // Standard video game actions pre-configured out of the box (Godot-inspired)
        g_actions.push_back({ "move_left",  { Key::A, Key::Left } });
        g_actions.push_back({ "move_right", { Key::D, Key::Right } });
        g_actions.push_back({ "move_up",    { Key::W, Key::Up } });
        g_actions.push_back({ "move_down",  { Key::S, Key::Down } });
        g_actions.push_back({ "jump",       { Key::Space, Key::W, Key::Up } });
        g_actions.push_back({ "attack",     { Key::J, Key::Z, Key::Enter } });
        g_actions.push_back({ "interact",   { Key::E, Key::F } });
        g_actions.push_back({ "sprint",     { Key::LeftShift, Key::RightShift } });
        g_actions.push_back({ "crouch",     { Key::LeftControl, Key::C, Key::S } });
        g_actions.push_back({ "dash",       { Key::K, Key::X } });
        g_actions.push_back({ "reload",     { Key::R } });
        g_actions.push_back({ "inventory",  { Key::Tab, Key::I } });
        g_actions.push_back({ "pause",      { Key::Escape, Key::P } });
        g_actions.push_back({ "ui_accept",  { Key::Enter, Key::Space } });
        g_actions.push_back({ "ui_cancel",  { Key::Escape, Key::Backspace } });
    }

    const std::vector<InputAction>& InputMap::GetActions() {
        EnsureInitialized();
        return g_actions;
    }

    std::vector<InputAction>& InputMap::GetActionsMutable() {
        EnsureInitialized();
        return g_actions;
    }

    const InputAction* InputMap::FindAction(const std::string& actionName) {
        EnsureInitialized();
        for (const auto& action : g_actions) {
            if (action.name == actionName) {
                return &action;
            }
        }
        return nullptr;
    }

    bool InputMap::HasAction(const std::string& actionName) {
        return FindAction(actionName) != nullptr;
    }

    bool InputMap::AddAction(const std::string& actionName) {
        EnsureInitialized();
        std::string cleanName = TrimString(actionName);
        if (cleanName.empty()) {
            TALOS_LOG_WARN("Cannot create an InputMap action with an empty name.");
            return false;
        }
        // Replace spaces with underscores for clean identifier naming
        for (char& c : cleanName) {
            if (c == ' ') c = '_';
        }
        if (HasAction(cleanName)) {
            TALOS_LOG_WARN("InputMap action '" + cleanName + "' already exists.");
            return false;
        }
        g_actions.push_back({ cleanName, {} });
        TALOS_LOG_INFO("InputMap: Created action '" + cleanName + "'.");
        return true;
    }

    bool InputMap::RemoveAction(const std::string& actionName) {
        EnsureInitialized();
        for (auto it = g_actions.begin(); it != g_actions.end(); ++it) {
            if (it->name == actionName) {
                g_actions.erase(it);
                TALOS_LOG_INFO("InputMap: Removed action '" + actionName + "'.");
                return true;
            }
        }
        return false;
    }

    bool InputMap::BindKey(const std::string& actionName, Key key) {
        if (key == Key::Unknown) return false;
        InputAction* action = FindActionMutable(actionName);
        if (!action) {
            TALOS_LOG_WARN("InputMap::BindKey failed: Action '" + actionName + "' does not exist.");
            return false;
        }
        for (Key existing : action->keys) {
            if (existing == key) {
                return false; // Already bound to this action
            }
        }
        action->keys.push_back(key);
        TALOS_LOG_INFO("InputMap: Bound key [" + std::string(Input::GetKeyName(key)) +
                       "] to action '" + actionName + "'.");
        return true;
    }

    bool InputMap::UnbindKey(const std::string& actionName, Key key) {
        InputAction* action = FindActionMutable(actionName);
        if (!action) return false;
        auto it = std::find(action->keys.begin(), action->keys.end(), key);
        if (it != action->keys.end()) {
            action->keys.erase(it);
            TALOS_LOG_INFO("InputMap: Unbound key [" + std::string(Input::GetKeyName(key)) +
                           "] from action '" + actionName + "'.");
            return true;
        }
        return false;
    }

    bool InputMap::ClearActionKeys(const std::string& actionName) {
        InputAction* action = FindActionMutable(actionName);
        if (!action) return false;
        action->keys.clear();
        return true;
    }

    bool InputMap::IsActionDown(const std::string& actionName) {
        const InputAction* action = FindAction(actionName);
        if (!action) return false;
        for (Key key : action->keys) {
            if (Input::IsKeyDown(key)) {
                return true;
            }
        }
        return false;
    }

    bool InputMap::IsActionPressed(const std::string& actionName) {
        const InputAction* action = FindAction(actionName);
        if (!action) return false;
        for (Key key : action->keys) {
            if (Input::IsKeyPressedOnce(key)) {
                return true;
            }
        }
        return false;
    }

    bool InputMap::IsActionReleased(const std::string& actionName) {
        const InputAction* action = FindAction(actionName);
        if (!action) return false;
        for (Key key : action->keys) {
            if (Input::IsKeyReleased(key)) {
                return true;
            }
        }
        return false;
    }

    float InputMap::GetAxis(const std::string& negativeAction, const std::string& positiveAction) {
        float axis = 0.0f;
        if (IsActionDown(negativeAction)) axis -= 1.0f;
        if (IsActionDown(positiveAction)) axis += 1.0f;
        return axis;
    }

    bool InputMap::Save(const std::string& filePath) {
        EnsureInitialized();
        try {
            std::filesystem::path p(filePath);
            if (p.has_parent_path()) {
                std::filesystem::create_directories(p.parent_path());
            }
        } catch (...) {
            // Proceed to open stream even if directory check fails
        }

        std::ofstream out(filePath, std::ios::trunc);
        if (!out.is_open()) {
            TALOS_LOG_ERROR("InputMap::Save failed: Unable to open '" + filePath + "' for writing.");
            return false;
        }

        out << "# Talos Engine - Project Input Map Configuration\n";
        out << "# Format: action_name = Key1, Key2, ...\n\n";

        for (const auto& action : g_actions) {
            out << action.name << " = ";
            for (size_t i = 0; i < action.keys.size(); ++i) {
                out << Input::GetKeyToken(action.keys[i]);
                if (i + 1 < action.keys.size()) {
                    out << ", ";
                }
            }
            out << "\n";
        }

        TALOS_LOG_INFO("Saved Input Map (" + std::to_string(g_actions.size()) +
                       " actions) to '" + filePath + "'.");
        return true;
    }

    bool InputMap::Load(const std::string& filePath) {
        std::ifstream in(filePath);
        if (!in.is_open()) {
            // Initialize defaults if no custom input_map.talos exists yet
            InitDefaults();
            return false;
        }

        std::vector<InputAction> loadedActions;
        std::string line;
        while (std::getline(in, line)) {
            std::string trimmed = TrimString(line);
            if (trimmed.empty() || trimmed[0] == '#') continue;

            size_t eqPos = trimmed.find('=');
            if (eqPos == std::string::npos) continue;

            std::string actionName = TrimString(trimmed.substr(0, eqPos));
            std::string keysPart   = TrimString(trimmed.substr(eqPos + 1));
            if (actionName.empty()) continue;

            InputAction action;
            action.name = actionName;

            std::stringstream ss(keysPart);
            std::string keyToken;
            while (std::getline(ss, keyToken, ',')) {
                Key parsedKey = Key::Unknown;
                if (Input::TryParseKeyToken(keyToken, parsedKey) && parsedKey != Key::Unknown) {
                    if (std::find(action.keys.begin(), action.keys.end(), parsedKey) == action.keys.end()) {
                        action.keys.push_back(parsedKey);
                    }
                }
            }

            loadedActions.push_back(action);
        }

        if (loadedActions.empty()) {
            InitDefaults();
            return false;
        }

        g_initialized = true;
        g_actions = std::move(loadedActions);
        TALOS_LOG_INFO("Loaded Input Map (" + std::to_string(g_actions.size()) +
                       " actions) from '" + filePath + "'.");
        return true;
    }

}
