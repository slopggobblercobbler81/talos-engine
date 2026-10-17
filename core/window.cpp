#include "core/window.h"
#include "core/logger.h"
#include "raylib.h"
#include <string>

namespace Talos::Core {

    void Window::Init(int width, int height, const char* title) {
        // Suppress verbose backend info logs so the developer console stays clean.
        SetTraceLogLevel(LOG_WARNING);

        // Initialize the underlying OpenGL context and OS window surface.
        InitWindow(width, height, title);

        // Enforce a 60 FPS cap so the main loop yields CPU time between frames
        // and maintains consistent frame pacing.
        SetTargetFPS(60);

        TALOS_LOG_INFO("Window initialized (" + std::to_string(width) + "x" +
                       std::to_string(height) + ") locked at 60 FPS");
    }

    void Window::Close() {
        TALOS_LOG_INFO("Closing window and releasing OpenGL context");
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