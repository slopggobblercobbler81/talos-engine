#include "core/input.h"
#include "raylib.h"

namespace Talos::Core {

    // Internal translation function kept private to this translation unit ('static').
    // Maps Talos::Core::Key values to the underlying backend's integer key constants.
    // If the input backend ever changes (e.g., to SDL or GLFW), only this table changes.
    static int ToBackendKey(Key key) {
        switch (key) {
            case Key::W:      return KEY_W;
            case Key::A:      return KEY_A;
            case Key::S:      return KEY_S;
            case Key::D:      return KEY_D;
            case Key::Up:     return KEY_UP;
            case Key::Down:   return KEY_DOWN;
            case Key::Left:   return KEY_LEFT;
            case Key::Right:  return KEY_RIGHT;
            case Key::Space:  return KEY_SPACE;
            case Key::Escape: return KEY_ESCAPE;
            default:          return KEY_NULL;
        }
    }

    bool Input::IsKeyDown(Key key) {
        // The leading '::' scope resolution operator explicitly invokes the global
        // backend function ::IsKeyDown(int) rather than recursively calling Input::IsKeyDown.
        return ::IsKeyDown(ToBackendKey(key));
    }

    bool Input::IsKeyPressed(Key key) {
        // Queries single-frame key press transition from the global backend function.
        return ::IsKeyPressed(ToBackendKey(key));
    }

}