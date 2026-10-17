#pragma once
#include <string>
#include "scene/scene.h"
#include "renderer/renderer2d.h"
#include "editor/gui/editor_gui.h"
#include "editor/history/editor_undo_redo.h"
#include "editor/dialogs/input_map_dialog.h"

namespace Talos::Editor {

    // =========================================================================
    // Top Editor Toolbar (Layer 5: Editor -> Toolbar Branch)
    // =========================================================================
    // Renders the top command bar (Play/Stop simulation toggle, temporary status
    // notification banner, + New Entity, Delete, Input Map, Save Scene, Load Scene),
    // analogous to Godot's EditorTitleBar / EditorRunBar.
    class EditorToolbar {
    public:
        // Displays a temporary notification message in the Top Toolbar.
        void ShowStatusMessage(const std::string& message, float durationSeconds = 2.0f);

        // Counts down the temporary notification timer each frame.
        void UpdateTimer(float delta);

        // Renders the top toolbar and handles button clicks.
        void Render(Scene::Scene& scene,
                    int windowWidth,
                    int windowHeight,
                    float consoleHeight,
                    const std::string& sceneFilePath,
                    bool isPlaying,
                    bool diagnosticOverlayEnabled,
                    Scene::EntityID& inOutSelectedEntityId,
                    const Renderer::Camera2DView& camera,
                    EditorGui& gui,
                    EditorUndoRedo& undoRedo,
                    InputMapDialog& inputMapDialog,
                    bool& outRequestStartPlay,
                    bool& outRequestStopPlay);

    private:
        int m_newEntityCounter = 1;
        std::string m_statusMessage;
        float m_statusTimer = 0.0f;
    };

}
