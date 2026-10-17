#include "core/window.h"
#include "raylib.h"

namespace Talos::Core {

    void Window::Init(int width, int height, const char* title) {
        InitWindow(width, height, title);
        SetTargetFPS(60);
    }

    void Window::Close() {
        CloseWindow();
    }

    bool Window::ShouldClose() {
        return WindowShouldClose();
    }

    float Window::GetDeltaTime() {
        return GetFrameTime();
    }
}