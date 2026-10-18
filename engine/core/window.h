// OS window lifecycle and frame timing (delta)
#pragma once

namespace Talos::Core {

    // =========================================================================
    // Window Subsystem (Layer 1: Core & Platform)
    // =========================================================================
    // Abstracts operating system window management and frame timing.
    // By isolating windowing calls behind this static interface, no upper layer
    // needs to include platform-specific or third-party graphics headers.
    class Window {
    public:
        // Creates the native OS window and locks the swap interval to a stable
        // target frame rate (60 FPS by default) to prevent unbounded CPU/GPU usage.
        static void Init(int width, int height, const char* title);

        // Releases the native window context and associated GPU resources.
        // Must be called once when the engine loop terminates.
        static void Close();

        // Polls whether the OS has requested application termination
        // (e.g., clicking the window 'X' button, Alt+F4, or pressing Escape).
        static bool ShouldClose();

        // Returns the elapsed time in seconds between the previous frame and
        // the current frame (commonly known as "delta time").
        // Multiplying movement velocities by this value guarantees frame-rate
        // independent simulation speed across different hardware.
        static float GetDeltaTime();
    };

}