#include "core/input.h"
#include "raylib.h"

namespace Talos::Core {

    namespace {

        struct KeyDescriptor {
            Key key;
            int backendCode;
            const char* token;
            const char* displayName;
        };

        // Unified lookup table mapping every engine Key to its backend code,
        // serialization token, and human-readable Editor badge label.
        constexpr KeyDescriptor KEY_TABLE[] = {
            // Alphabet (A - Z)
            { Key::A, KEY_A, "A", "A" },
            { Key::B, KEY_B, "B", "B" },
            { Key::C, KEY_C, "C", "C" },
            { Key::D, KEY_D, "D", "D" },
            { Key::E, KEY_E, "E", "E" },
            { Key::F, KEY_F, "F", "F" },
            { Key::G, KEY_G, "G", "G" },
            { Key::H, KEY_H, "H", "H" },
            { Key::I, KEY_I, "I", "I" },
            { Key::J, KEY_J, "J", "J" },
            { Key::K, KEY_K, "K", "K" },
            { Key::L, KEY_L, "L", "L" },
            { Key::M, KEY_M, "M", "M" },
            { Key::N, KEY_N, "N", "N" },
            { Key::O, KEY_O, "O", "O" },
            { Key::P, KEY_P, "P", "P" },
            { Key::Q, KEY_Q, "Q", "Q" },
            { Key::R, KEY_R, "R", "R" },
            { Key::S, KEY_S, "S", "S" },
            { Key::T, KEY_T, "T", "T" },
            { Key::U, KEY_U, "U", "U" },
            { Key::V, KEY_V, "V", "V" },
            { Key::W, KEY_W, "W", "W" },
            { Key::X, KEY_X, "X", "X" },
            { Key::Y, KEY_Y, "Y", "Y" },
            { Key::Z, KEY_Z, "Z", "Z" },

            // Top-row Numbers (0 - 9)
            { Key::D0, KEY_ZERO,  "0", "0" },
            { Key::D1, KEY_ONE,   "1", "1" },
            { Key::D2, KEY_TWO,   "2", "2" },
            { Key::D3, KEY_THREE, "3", "3" },
            { Key::D4, KEY_FOUR,  "4", "4" },
            { Key::D5, KEY_FIVE,  "5", "5" },
            { Key::D6, KEY_SIX,   "6", "6" },
            { Key::D7, KEY_SEVEN, "7", "7" },
            { Key::D8, KEY_EIGHT, "8", "8" },
            { Key::D9, KEY_NINE,  "9", "9" },

            // Directional Arrow keys
            { Key::Up,    KEY_UP,    "Up",    "Up Arrow" },
            { Key::Down,  KEY_DOWN,  "Down",  "Down Arrow" },
            { Key::Left,  KEY_LEFT,  "Left",  "Left Arrow" },
            { Key::Right, KEY_RIGHT, "Right", "Right Arrow" },

            // Action & Navigation keys
            { Key::Space,     KEY_SPACE,     "Space",     "Space" },
            { Key::Enter,     KEY_ENTER,     "Enter",     "Enter" },
            { Key::Escape,    KEY_ESCAPE,    "Escape",    "Escape" },
            { Key::Tab,       KEY_TAB,       "Tab",       "Tab" },
            { Key::Backspace, KEY_BACKSPACE, "Backspace", "Backspace" },
            { Key::Delete,    KEY_DELETE,    "Delete",    "Delete" },

            // Modifiers
            { Key::LeftShift,    KEY_LEFT_SHIFT,    "LeftShift",    "Left Shift" },
            { Key::RightShift,   KEY_RIGHT_SHIFT,   "RightShift",   "Right Shift" },
            { Key::LeftControl,  KEY_LEFT_CONTROL,  "LeftControl",  "Left Ctrl" },
            { Key::RightControl, KEY_RIGHT_CONTROL, "RightControl", "Right Ctrl" },
            { Key::LeftAlt,      KEY_LEFT_ALT,      "LeftAlt",      "Left Alt" },
            { Key::RightAlt,     KEY_RIGHT_ALT,     "RightAlt",     "Right Alt" },

            // Function keys (F1 - F12)
            { Key::F1,  KEY_F1,  "F1",  "F1" },
            { Key::F2,  KEY_F2,  "F2",  "F2" },
            { Key::F3,  KEY_F3,  "F3",  "F3" },
            { Key::F4,  KEY_F4,  "F4",  "F4" },
            { Key::F5,  KEY_F5,  "F5",  "F5" },
            { Key::F6,  KEY_F6,  "F6",  "F6" },
            { Key::F7,  KEY_F7,  "F7",  "F7" },
            { Key::F8,  KEY_F8,  "F8",  "F8" },
            { Key::F9,  KEY_F9,  "F9",  "F9" },
            { Key::F10, KEY_F10, "F10", "F10" },
            { Key::F11, KEY_F11, "F11", "F11" },
            { Key::F12, KEY_F12, "F12", "F12" }
        };

        constexpr size_t KEY_TABLE_COUNT = sizeof(KEY_TABLE) / sizeof(KEY_TABLE[0]);

        const KeyDescriptor* FindDescriptor(Key key) {
            for (size_t i = 0; i < KEY_TABLE_COUNT; ++i) {
                if (KEY_TABLE[i].key == key) {
                    return &KEY_TABLE[i];
                }
            }
            return nullptr;
        }

        int ToBackendKey(Key key) {
            const KeyDescriptor* desc = FindDescriptor(key);
            return desc ? desc->backendCode : KEY_NULL;
        }

        int ToBackendMouseButton(MouseButton button) {
            switch (button) {
                case MouseButton::Left:   return MOUSE_BUTTON_LEFT;
                case MouseButton::Right:  return MOUSE_BUTTON_RIGHT;
                case MouseButton::Middle: return MOUSE_BUTTON_MIDDLE;
                default:                  return MOUSE_BUTTON_LEFT;
            }
        }

        std::string TrimToken(const std::string& str) {
            const char* ws = " \t\r\n";
            size_t first = str.find_first_not_of(ws);
            if (first == std::string::npos) return "";
            size_t last = str.find_last_not_of(ws);
            return str.substr(first, last - first + 1);
        }

    } // anonymous namespace

    bool Input::IsKeyDown(Key key) {
        return ::IsKeyDown(ToBackendKey(key));
    }

    bool Input::IsKeyPressed(Key key) {
        int backendKey = ToBackendKey(key);
        return ::IsKeyPressed(backendKey) || ::IsKeyPressedRepeat(backendKey);
    }

    bool Input::IsKeyPressedOnce(Key key) {
        return ::IsKeyPressed(ToBackendKey(key));
    }

    bool Input::IsKeyReleased(Key key) {
        return ::IsKeyReleased(ToBackendKey(key));
    }

    bool Input::PollAnyKeyPressed(Key& outKey) {
        for (size_t i = 0; i < KEY_TABLE_COUNT; ++i) {
            if (::IsKeyPressed(KEY_TABLE[i].backendCode)) {
                outKey = KEY_TABLE[i].key;
                return true;
            }
        }
        return false;
    }

    const char* Input::GetKeyName(Key key) {
        const KeyDescriptor* desc = FindDescriptor(key);
        return desc ? desc->displayName : "Unknown";
    }

    const char* Input::GetKeyToken(Key key) {
        const KeyDescriptor* desc = FindDescriptor(key);
        return desc ? desc->token : "Unknown";
    }

    bool Input::TryParseKeyToken(const std::string& token, Key& outKey) {
        std::string trimmed = TrimToken(token);
        for (size_t i = 0; i < KEY_TABLE_COUNT; ++i) {
            if (trimmed == KEY_TABLE[i].token || trimmed == KEY_TABLE[i].displayName) {
                outKey = KEY_TABLE[i].key;
                return true;
            }
        }
        outKey = Key::Unknown;
        return false;
    }

    bool Input::IsActionDown(const std::string& actionName) {
        return InputMap::IsActionDown(actionName);
    }

    bool Input::IsActionPressed(const std::string& actionName) {
        return InputMap::IsActionPressed(actionName);
    }

    bool Input::IsActionReleased(const std::string& actionName) {
        return InputMap::IsActionReleased(actionName);
    }

    float Input::GetAxis(const std::string& negativeAction, const std::string& positiveAction) {
        return InputMap::GetAxis(negativeAction, positiveAction);
    }

    int Input::GetCharPressed() {
        return ::GetCharPressed();
    }

    float Input::GetMouseX() {
        return static_cast<float>(::GetMouseX());
    }

    float Input::GetMouseY() {
        return static_cast<float>(::GetMouseY());
    }

    float Input::GetMouseWheelMove() {
        return ::GetMouseWheelMove();
    }

    bool Input::IsMouseButtonDown(MouseButton button) {
        return ::IsMouseButtonDown(ToBackendMouseButton(button));
    }

    bool Input::IsMouseButtonPressed(MouseButton button) {
        return ::IsMouseButtonPressed(ToBackendMouseButton(button));
    }

    bool Input::IsMouseButtonReleased(MouseButton button) {
        return ::IsMouseButtonReleased(ToBackendMouseButton(button));
    }

}