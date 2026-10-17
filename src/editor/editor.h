#pragma once
#include <vector>
#include <string>
#include "scene/scene.h"
#include "renderer/renderer2d.h"

namespace Talos::Editor {

    // =========================================================================
    // Visual Editor Subsystem (Layer 5: Editor Layer)
    // =========================================================================
    // Sits at the very top of the engine architecture. The Editor inspects and
    // mutates a Scene instance, handles viewport mouse picking/dragging, and manages
    // Play/Stop simulation state snapshots.
    //
    // Architectural Note: Lower layers (Core, Renderer, Physics, Scene) have zero
    // awareness of this class. A standalone game build simply omits the Editor layer.
    class Editor {
    public:
        explicit Editor(int windowWidth, int windowHeight);

        // Processes Editor UI interactions, viewport entity picking/dragging,
        // and advances the Scene simulation when Play mode is active.
        void Update(Scene::Scene& scene, float delta);

        // Renders the 2D viewport grid, the Scene entities, selection gizmos,
        // and all Editor panels (Toolbar, Entity List, and First-Level Inspector).
        void Render(Scene::Scene& scene);

        // Returns true when the game simulation is actively running (Play Mode).
        bool IsPlaying() const { return m_isPlaying; }

        // Selects an entity by ID so its properties appear in the Inspector.
        void SelectEntity(Scene::EntityID id) { m_selectedEntityId = id; }

    private:
        // State transition helpers for instant Play/Stop iteration without compilation
        void StartPlayMode(const Scene::Scene& scene);
        void StopPlayMode(Scene::Scene& scene);

        // Viewport interaction (mouse picking & entity dragging in Edit Mode)
        void HandleViewportInput(Scene::Scene& scene);

        // Immediate-mode UI panel rendering helpers
        void RenderViewportGrid() const;
        void RenderSelectionGizmo(const Scene::Scene& scene) const;
        void RenderTopToolbar(Scene::Scene& scene);
        void RenderEntityListPanel(Scene::Scene& scene);
        void RenderInspectorPanel(Scene::Scene& scene);

        // Lightweight immediate-mode UI widget primitives built strictly on Core::Input & Renderer2D
        bool DrawButton(float x, float y, float w, float h, const char* label,
                        Renderer::ColorRGBA bgColor, Renderer::ColorRGBA hoverColor);
        bool DrawCheckbox(float x, float y, const char* label, bool& value);
        void DrawFloatSlider(float x, float y, float w, const char* label,
                             float& value, float minVal, float maxVal, int sliderId);
        void DrawIntSlider(float x, float y, float w, const char* label,
                           int& value, int minVal, int maxVal, int sliderId);
        void DrawColorPickerRow(float x, float y, float w, const char* label,
                                Renderer::ColorRGBA& color, int baseSliderId);
        void DrawTextInputBox(float x, float y, float w, const char* label,
                              std::string& textBuffer, int fieldId);

    private:
        int m_windowWidth = 1360;
        int m_windowHeight = 780;

        // Layout metrics (in screen pixels)
        static constexpr float TOOLBAR_HEIGHT = 46.0f;
        static constexpr float LEFT_PANEL_WIDTH = 230.0f;
        static constexpr float RIGHT_PANEL_WIDTH = 350.0f;

        // Play / Edit simulation state
        bool m_isPlaying = false;
        std::vector<Scene::Entity> m_sceneSnapshot; // Saved before Play, restored on Stop

        // Selection and Viewport dragging state
        Scene::EntityID m_selectedEntityId = 0;
        bool m_isDraggingEntity = false;
        float m_dragOffsetX = 0.0f;
        float m_dragOffsetY = 0.0f;

        // Immediate-mode UI interaction state (active slider or focused text input field)
        int m_activeSliderId = -1;
        int m_focusedTextFieldId = -1;
        int m_newEntityCounter = 1;
    };

}

