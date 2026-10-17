#include "core/input.h"
#include "raylib.h"

namespace Talos::Core {

    // Internal translation function kept private to this translation unit ('static').
    // Maps Talos::Core::Key values to the underlying backend's integer key constants.
    static int ToBackendKey(Key key) {
        switch (key) {
            case Key::W:         return KEY_W;
            case Key::A:         return KEY_A;
            case Key::S:         return KEY_S;
            case Key::D:         return KEY_D;
            case Key::Up:        return KEY_UP;
            case Key::Down:      return KEY_DOWN;
            case Key::Left:      return KEY_LEFT;
            case Key::Right:     return KEY_RIGHT;
            case Key::Space:     return KEY_SPACE;
            case Key::Escape:    return KEY_ESCAPE;
            case Key::Backspace: return KEY_BACKSPACE;
            case Key::Delete:    return KEY_DELETE;
            case Key::Enter:     return KEY_ENTER;
            default:             return KEY_NULL;
        }
    }

    // Internal translation function mapping engine MouseButton to backend mouse constants.
    static int ToBackendMouseButton(MouseButton button) {
        switch (button) {
            case MouseButton::Left:   return MOUSE_BUTTON_LEFT;
            case MouseButton::Right:  return MOUSE_BUTTON_RIGHT;
            case MouseButton::Middle: return MOUSE_BUTTON_MIDDLE;
            default:                  return MOUSE_BUTTON_LEFT;
        }
    }

    bool Input::IsKeyDown(Key key) {
        // The leading '::' scope resolution operator explicitly invokes the global
        // backend function ::IsKeyDown(int) rather than recursively calling Input::IsKeyDown.
        return ::IsKeyDown(ToBackendKey(key));
    }

    bool Input::IsKeyPressed(Key key) {
        return ::IsKeyPressed(ToBackendKey(key));
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