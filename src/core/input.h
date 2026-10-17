#pragma once

namespace Talos::Core {
    // Engine-native key codes (isolated from the underlying backend)
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

    // Single point of access for keyboard and mouse input
    class Input {
    public:
        static bool IsKeyDown(Key key);
        static bool IsKeyPressed(Key key);
    };
}
