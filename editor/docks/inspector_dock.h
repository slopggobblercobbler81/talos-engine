#pragma once
#include "scene/scene.h"
#include "editor/gui/editor_gui.h"

namespace Talos::Editor {

    // =========================================================================
    // First-Level Inspector Dock (Layer 5: Editor -> Docks Branch)
    // =========================================================================
    // Renders the right-side FIRST-LEVEL INSPECTOR dock, exposing Transform 2D,
    // Sprite 2D, Text Label, Collider 2D, and Movement 2D properties directly
    // without nested sub-resource menus.
    class InspectorDock {
    public:
        void Render(Scene::Scene& scene,
                    int windowWidth,
                    int windowHeight,
                    Scene::EntityID selectedEntityId,
                    EditorGui& gui);

    private:
        float m_scrollOffset = 0.0f;
        float m_contentHeight = 0.0f;
    };

}
