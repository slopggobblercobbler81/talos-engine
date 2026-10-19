// Left-side scene tree dock listing entities with search/tag filtering and reordering
#pragma once
#include <string>
#include "scene/scene.h"
#include "editor/gui/editor_gui.h"
#include "editor/history/editor_undo_redo.h"

namespace Talos::Editor {

    // =========================================================================
    // Scene Tree Dock (Layer 5: Editor -> Docks Branch)
    // =========================================================================
    // Renders the left-side SCENE ENTITIES hierarchy dock, providing live search
    // by entity name or tag, quick tag filter chips, and entity list reordering
    // (via header/inline buttons, Alt+Up/Down, or drag-and-drop) with Undo/Redo.
    class SceneTreeDock {
    public:
        static constexpr int SEARCH_FIELD_ID = 14;

        void BindTextFields(EditorGui& gui);

        void Render(Scene::Scene& scene,
                    int windowHeight,
                    float bottomPanelHeight,
                    Scene::EntityID& inOutSelectedEntityId,
                    bool isPlaying,
                    EditorGui& gui,
                    EditorUndoRedo& undoRedo,
                    std::string* outStatusMessage = nullptr);

        const std::string& GetSearchQuery() const { return m_searchQuery; }
        const std::string& GetTagFilter() const { return m_tagFilter; }
        void ClearFilters();

    private:
        std::string m_searchQuery;
        std::string m_tagFilter;
        float m_scrollOffset = 0.0f;

        // Drag-and-drop row reordering state
        Scene::EntityID m_draggingEntityId = 0;
        float m_dragStartMouseY = 0.0f;
        bool m_isDraggingRow = false;
        int m_dropTargetEntityIndex = -1;
    };

}

