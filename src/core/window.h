#pragma once

namespace Talos::Core {

    // Everything related to window can be accessed ONLY for here
    class Window {
    public:
        static void Init(int width, int height, const char* title);
        static void Close();
        static bool ShouldClose();
        static float GetDeltaTime();
    };
    
}