#include "core/window.h"
#include "raylib.h"

namespace Talos::Core {

    void Window::Init(int width, int height, const char* title) {
        // Suppress verbose backend info logs so the developer console stays clean.
        SetTraceLogLevel(LOG_WARNING);

        // Initialize the underlying OpenGL context and OS window surface.
        InitWindow(width, height, title);

        // Enforce a 60 FPS cap so the main loop yields CPU time between frames
        // and maintains consistent frame pacing.
        SetTargetFPS(60);
    }

    void Window::Close() {
        // Cleanly destroy the OpenGL context and release OS window handles.
        CloseWindow();
    }

    bool Window::ShouldClose() {
        // Query the platform backend for pending window close events.
        return WindowShouldClose();
    }

    float Window::GetDeltaTime() {
        // Retrieve the exact duration (in seconds) of the last completed frame.
        return GetFrameTime();
    }

}