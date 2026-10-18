// [F3] diagnostic HUD with per-component telemetry
#pragma once
#include "scene/scene.h"

namespace Talos::Editor {

    // =========================================================================
    // Viewport Diagnostic Overlay [F3] (Layer 5: Editor -> Viewport Branch)
    // =========================================================================
    // Renders the real-time per-component resource consumption telemetry card
    // (CPU time in microseconds, CPU load share bar, exact RAM & GPU VRAM bytes,
    // operations count, and selected entity breakdown) during Play Mode.
    class DiagnosticOverlay {
    public:
        // Updates the exponentially smoothed frame delta for readable FPS display.
        void UpdateFrameDelta(float delta);

        // Toggles the overlay on/off (called when F3 is pressed during Play Mode).
        // Note: State persists across Stop -> Play transitions unless explicitly toggled off.
        bool Toggle();

        bool IsEnabled() const { return m_enabled; }

        // Renders the diagnostic telemetry overlay inside the central viewport.
        void Render(const Scene::Scene& scene,
                    int windowWidth,
                    int windowHeight,
                    float consoleHeight,
                    Scene::EntityID selectedEntityId) const;

    private:
        bool m_enabled = false;
        float m_smoothedFrameDelta = 0.01667f;
    };

}
