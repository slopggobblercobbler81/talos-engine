#include "editor/editor.h"
#include "core/logger.h"
#include "core/input.h"

namespace Talos::Editor {

    Editor::Editor(int windowWidth, int windowHeight, const std::string& sceneFilePath)
        : m_windowWidth(windowWidth), m_windowHeight(windowHeight), m_sceneFilePath(sceneFilePath) {
        m_inputMapDialog.Initialize("examples/scenes/input_map.talos");
        TALOS_LOG_INFO("Central Editor Orchestrator initialized (Godot-style modular tree).");
    }

    // =========================================================================
    // Universal Undo / Redo Delegation
    // =========================================================================

    void Editor::Undo(Scene::Scene& scene) {
        if (m_isPlaying) return;

        m_gui.CancelSliderDrag();
        m_viewport.CancelEntityDrag();

        if (m_undoRedo.Undo(scene,
                            m_selectedEntityId,
                            m_gui.GetTextCursorPosRef(),
                            m_gui.GetTextSelectionAnchorRef(),
                            m_isPlaying)) {
            m_toolbar.ShowStatusMessage("Undo", 1.2f);
        } else {
            m_toolbar.ShowStatusMessage("Nothing to Undo", 1.2f);
        }
    }

    void Editor::Redo(Scene::Scene& scene) {
        if (m_isPlaying) return;

        m_gui.CancelSliderDrag();
        m_viewport.CancelEntityDrag();

        if (m_undoRedo.Redo(scene,
                            m_selectedEntityId,
                            m_gui.GetTextCursorPosRef(),
                            m_gui.GetTextSelectionAnchorRef(),
                            m_isPlaying)) {
            m_toolbar.ShowStatusMessage("Redo", 1.2f);
        } else {
            m_toolbar.ShowStatusMessage("Nothing to Redo", 1.2f);
        }
    }

    // =========================================================================
    // Play / Stop Simulation Mode Lifecycle
    // =========================================================================

    void Editor::StartPlayMode(Scene::Scene& scene) {
        if (m_isPlaying) return;

        if (m_undoRedo.HasPendingSnapshot()) {
            m_undoRedo.CommitContinuousEdit(scene);
        }

        m_editSnapshot = scene.GetEntities();
        m_isPlaying = true;
        m_gui.CancelSliderDrag();
        m_gui.UnfocusTextField();
        m_viewport.CancelEntityDrag();
        m_inputMapDialog.Close(m_gui);

        m_toolbar.ShowStatusMessage("Simulation Started (Play Mode)", 2.0f);
        TALOS_LOG_INFO("Entering Play Mode (snapshot saved: " + std::to_string(m_editSnapshot.size()) + " entities)");
    }

    void Editor::StopPlayMode(Scene::Scene& scene) {
        if (!m_isPlaying) return;

        scene.GetEntities() = m_editSnapshot;
        m_isPlaying = false;
        m_gui.CancelSliderDrag();
        m_gui.UnfocusTextField();
        m_viewport.CancelEntityDrag();

        if (m_selectedEntityId != 0 && !scene.FindEntityById(m_selectedEntityId)) {
            m_selectedEntityId = scene.GetEntities().empty() ? 0 : scene.GetEntities().front().id;
        }

        m_toolbar.ShowStatusMessage("Simulation Stopped (Scene Restored)", 2.0f);
        TALOS_LOG_INFO("Exiting Play Mode (scene restored to pre-play snapshot)");
    }

    // =========================================================================
    // Central Frame Update
    // =========================================================================

    void Editor::Update(Scene::Scene& scene, float delta) {
        m_toolbar.UpdateTimer(delta);
        m_diagnosticOverlay.UpdateFrameDelta(delta);
        m_gui.SetFrameContext(&scene, &m_undoRedo, m_selectedEntityId, m_isPlaying);

        // Release active continuous drags on mouse button release
        if (!Core::Input::IsMouseButtonDown(Core::MouseButton::Left)) {
            if (m_undoRedo.HasPendingSnapshot()) {
                m_undoRedo.CommitContinuousEdit(scene);
            }
            m_gui.OnLeftMouseReleased();
            m_viewport.OnMouseReleased(true, false);
        }

        if (!Core::Input::IsMouseButtonDown(Core::MouseButton::Middle) &&
            !Core::Input::IsMouseButtonDown(Core::MouseButton::Right)) {
            m_viewport.OnMouseReleased(false, true);
        }

        // In Play Mode, advance the live game simulation (Movement + Collisions)
        // and allow [F3] to toggle the Diagnostic Overlay.
        if (m_isPlaying) {
            if (Core::Input::IsKeyPressedOnce(Core::Key::F3)) {
                bool nowEnabled = m_diagnosticOverlay.Toggle();
                m_toolbar.ShowStatusMessage(
                    nowEnabled ? "Diagnostic Overlay Enabled [F3]" : "Diagnostic Overlay Disabled [F3]",
                    1.5f
                );
            }
            scene.Update(delta);
            return;
        }

        // Universal Undo / Redo keyboard shortcuts (Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z) in Edit Mode
        bool ctrlDown = Core::Input::IsKeyDown(Core::Key::LeftControl) ||
                        Core::Input::IsKeyDown(Core::Key::RightControl);
        bool shiftDown = Core::Input::IsKeyDown(Core::Key::LeftShift) ||
                         Core::Input::IsKeyDown(Core::Key::RightShift);

        if (ctrlDown && !m_inputMapDialog.IsListeningForKey()) {
            while (Core::Input::GetCharPressed() > 0) {}

            if (Core::Input::IsKeyPressed(Core::Key::Z)) {
                if (shiftDown) {
                    Redo(scene);
                } else {
                    Undo(scene);
                }
                return;
            }
            if (Core::Input::IsKeyPressed(Core::Key::Y)) {
                Redo(scene);
                return;
            }
        }

        // Modal Input Map interactive key listening takes priority over normal keyboard shortcuts
        if (m_inputMapDialog.IsOpen()) {
            std::string modalStatus;
            if (m_inputMapDialog.UpdateKeyListening(modalStatus)) {
                if (!modalStatus.empty()) {
                    m_toolbar.ShowStatusMessage(modalStatus, 2.0f);
                }
                return;
            }
            if (Core::Input::IsKeyPressedOnce(Core::Key::Escape) && m_gui.GetFocusedTextFieldId() == -1) {
                m_inputMapDialog.Close(m_gui);
                return;
            }
        }

        // Process keyboard typing for any focused text field (Inspector or Input Map modal)
        bool enterPressedOnField4 = false;
        m_gui.UpdateFocusedTextField(scene,
                                     m_selectedEntityId,
                                     m_inputMapDialog.GetNewActionNameBuffer(),
                                     m_isPlaying,
                                     ctrlDown,
                                     shiftDown,
                                     enterPressedOnField4);
        if (enterPressedOnField4) {
            std::string addActionStatus;
            m_inputMapDialog.SubmitNewAction(m_gui, addActionStatus);
            if (!addActionStatus.empty()) {
                m_toolbar.ShowStatusMessage(addActionStatus, 2.0f);
            }
        }

        // Block 2D viewport navigation and entity picking while a modal dialog is open
        if (m_inputMapDialog.IsOpen()) {
            return;
        }

        // 2D Canvas Viewport shortcuts & mouse interaction in Edit Mode
        float consoleH = m_consoleDock.GetPanelHeight();
        if (m_gui.GetFocusedTextFieldId() == -1 && !ctrlDown) {
            if (m_selectedEntityId != 0 && Core::Input::IsKeyPressedOnce(Core::Key::Delete)) {
                m_undoRedo.PushUndoState(scene, m_selectedEntityId,
                                         m_gui.GetTextCursorPos(), m_gui.GetTextSelectionAnchor(), m_isPlaying);
                const Scene::Entity* target = scene.FindEntityById(m_selectedEntityId);
                std::string deletedName = target ? target->name : std::to_string(m_selectedEntityId);
                scene.RemoveEntity(m_selectedEntityId);
                m_selectedEntityId = 0;
                m_toolbar.ShowStatusMessage("Deleted '" + deletedName + "'", 1.5f);
                TALOS_LOG_INFO("Deleted entity '" + deletedName + "' via keyboard shortcut.");
            } else if (Core::Input::IsKeyPressedOnce(Core::Key::F)) {
                m_toolbar.ShowStatusMessage(
                    m_viewport.FocusSelectedEntity(scene, m_selectedEntityId, m_windowWidth, m_windowHeight, consoleH),
                    1.2f
                );
            }
        }

        m_viewport.HandleInput(scene,
                               m_windowWidth,
                               m_windowHeight,
                               consoleH,
                               m_selectedEntityId,
                               m_gui,
                               m_undoRedo,
                               m_isPlaying);
    }

    // =========================================================================
    // Central Frame Render
    // =========================================================================

    void Editor::Render(Scene::Scene& scene) {
        m_gui.SetFrameContext(&scene, &m_undoRedo, m_selectedEntityId, m_isPlaying);
        float consoleH = m_consoleDock.GetPanelHeight();

        // 1. Central 2D Canvas Viewport (Clipped World Space: Grid + Scene + Selection Gizmos)
        m_viewport.RenderWorld(scene, m_windowWidth, m_windowHeight, consoleH, m_selectedEntityId, m_isPlaying);

        // 2. Viewport Overlays (Screen Space: Camera HUD in Edit Mode, [F3] Telemetry in Play Mode)
        if (!m_isPlaying) {
            std::string camStatus;
            m_viewport.RenderCameraOverlay(scene, m_windowWidth, m_windowHeight, consoleH,
                                           m_selectedEntityId, m_gui, camStatus);
            if (!camStatus.empty()) {
                m_toolbar.ShowStatusMessage(camStatus, 1.2f);
            }
        } else if (m_diagnosticOverlay.IsEnabled()) {
            m_diagnosticOverlay.Render(scene, m_windowWidth, m_windowHeight, consoleH, m_selectedEntityId);
        }

        // 3. Docked Editor Panels & Top Toolbar
        bool requestStartPlay = false;
        bool requestStopPlay = false;
        m_toolbar.Render(scene,
                         m_windowWidth,
                         m_windowHeight,
                         consoleH,
                         m_sceneFilePath,
                         m_isPlaying,
                         m_diagnosticOverlay.IsEnabled(),
                         m_selectedEntityId,
                         m_viewport.GetCamera(),
                         m_gui,
                         m_undoRedo,
                         m_inputMapDialog,
                         requestStartPlay,
                         requestStopPlay);

        if (requestStartPlay) StartPlayMode(scene);
        if (requestStopPlay)  StopPlayMode(scene);

        m_sceneTreeDock.Render(scene, m_windowHeight, m_selectedEntityId, m_gui);
        m_inspectorDock.Render(scene, m_windowWidth, m_windowHeight, m_selectedEntityId, m_gui);
        m_consoleDock.Render(m_windowWidth, m_windowHeight, m_gui);

        // 4. Project Modal Dialogs (rendered on top of docks and viewport)
        if (m_inputMapDialog.IsOpen()) {
            std::string modalStatus;
            m_inputMapDialog.Render(m_windowWidth, m_windowHeight, consoleH, m_gui, modalStatus);
            if (!modalStatus.empty()) {
                m_toolbar.ShowStatusMessage(modalStatus, 2.0f);
            }
        }
    }

}
