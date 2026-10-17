#include "core/input.h"
#include "raylib.h"


namespace Talos::Core {

    // Internal helper to translate Talos Key to backend key code
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
        return ::IsKeyDown(ToBackendKey(key));
    }

    bool Input::IsKeyPressed(Key key) {
        return ::IsKeyPressed(ToBackendKey(key));
    }
}