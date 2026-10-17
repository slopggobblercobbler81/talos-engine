#pragma once
#include "scene/scene.h"
#include "editor/gui/editor_gui.h"

namespace Talos::Editor {

    // =========================================================================
    // Scene Tree Dock (Layer 5: Editor -> Docks Branch)
    // =========================================================================
    // Renders the left-side SCENE ENTITIES hierarchy dock, displaying each
    // entity's texture thumbnail or color swatch and handling click selection.
    class SceneTreeDock {
    public:
        void Render(Scene::Scene& scene,
                    int windowHeight,
                    Scene::EntityID& inOutSelectedEntityId,
                    EditorGui& gui);
    };

}
