// Central editor orchestrator (EditorNode): owns docks, viewport, toolbar, play/stop and undo
#pragma once
#include <string>
#include <vector>
#include "scene/scene.h"
#include "editor/gui/editor_gui.h"
#include "editor/history/editor_undo_redo.h"
#include "editor/toolbar/editor_toolbar.h"
#include "editor/viewport/canvas_viewport.h"
#include "editor/viewport/diagnostic_overlay.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/docks/inspector_dock.h"
#include "editor/docks/console_dock.h"
#include "editor/docks/asset_browser_dock.h"
#include "editor/dialogs/input_map_dialog.h"
#include "editor/dialogs/project_dialog.h"

namespace Talos::Editor {

    // =========================================================================
    // Talos Central Editor Orchestrator (Layer 5: Editor Root / EditorNode)
    // =========================================================================
    // Acts as the central coordinator (analogous to Godot's EditorNode), owning
    // and orchestrating the specialized Editor branches:
    //   - editor/gui/      : Immediate-mode UI widgets & text controller (EditorGui)
    //   - editor/history/  : Universal Undo/Redo & drag snapshot manager (EditorUndoRedo)
    //   - editor/toolbar/  : Top command bar & status banner (EditorToolbar)
    //   - editor/viewport/ : 2D Canvas Viewport (CanvasViewport) & F3 Telemetry (DiagnosticOverlay)
    //   - editor/docks/    : Scene Tree, First-Level Inspector, Debug Console, and Asset Browser docks
    //   - editor/dialogs/  : Project modal dialogs (ProjectDialog, InputMapDialog)
    class Editor {
    public:
        Editor(int windowWidth,
               int windowHeight,
               const std::string& sceneFilePath = "examples/scenes/demo_scene.talsc",
               const std::string& projectFilePath = "examples/project.talos");
        ~Editor() = default;

        void SelectEntity(Scene::EntityID id) { m_selectedEntityId = id; }
        Scene::EntityID GetSelectedEntity() const { return m_selectedEntityId; }

        const std::string& GetActiveSceneFilePath() const { return m_sceneFilePath; }
        const Scene::ProjectConfig& GetProjectConfig() const { return m_projectDialog.GetConfig(); }

        // Processes editor shortcuts, modal key listening, text editing, and 2D viewport navigation.
        void Update(Scene::Scene& scene, float delta);

        // Renders the 2D Canvas Viewport, Docked Panels, Top Toolbar, and Modal Dialogs.
        void Render(Scene::Scene& scene);

        // Returns true when the Editor is in live Play Simulation Mode.
        bool IsPlaying() const { return m_isPlaying; }

        // Universal Undo / Redo operations (Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z)
        void Undo(Scene::Scene& scene);
        void Redo(Scene::Scene& scene);

        // Clones the currently selected entity in Edit Mode with Undo/Redo support (Ctrl+D).
        bool DuplicateSelectedEntity(Scene::Scene& scene);

    private:
        void StartPlayMode(Scene::Scene& scene);
        void StopPlayMode(Scene::Scene& scene);

    private:
        int m_windowWidth;
        int m_windowHeight;
        std::string m_sceneFilePath;
        Scene::EntityID m_selectedEntityId = 0;

        // Play / Stop Simulation Mode state
        bool m_isPlaying = false;
        std::vector<Scene::Entity> m_editSnapshot;

        // Specialized Editor Subsystem Branches
        EditorGui         m_gui;
        EditorUndoRedo    m_undoRedo;
        EditorToolbar     m_toolbar;
        CanvasViewport    m_viewport;
        DiagnosticOverlay m_diagnosticOverlay;
        SceneTreeDock     m_sceneTreeDock;
        InspectorDock     m_inspectorDock;
        ConsoleDock       m_consoleDock;
        AssetBrowserDock  m_assetBrowserDock;
        ProjectDialog     m_projectDialog;
        InputMapDialog    m_inputMapDialog;
    };

}
