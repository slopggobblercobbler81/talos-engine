#include "editor/editor.h"
#include "scene/scene_serializer.h"
#include "audio/audio_system.h"
#include "core/logger.h"
#include "core/input.h"

namespace Talos::Editor {

    Editor::Editor(int windowWidth,
                   int windowHeight,
                   const std::string& sceneFilePath,
                   const std::string& projectFilePath)
        : m_windowWidth(windowWidth), m_windowHeight(windowHeight), m_sceneFilePath(sceneFilePath) {
        m_inputMapDialog.Initialize("examples/scenes/input_map.talos");
        if (!Audio::AudioSystem::LoadConfig("examples/scenes/audio_settings.talos")) {
            Audio::AudioSystem::SaveConfig("examples/scenes/audio_settings.talos");
        }
        m_projectDialog.Initialize(projectFilePath, m_sceneFilePath);
        m_projectDialog.BindTextFields(m_gui);
        m_sceneTreeDock.BindTextFields(m_gui);
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

    bool Editor::DuplicateSelectedEntity(Scene::Scene& scene) {
        if (m_isPlaying) return false;

        const Scene::Entity* source = (m_selectedEntityId != 0) ? scene.FindEntityById(m_selectedEntityId) : nullptr;
        if (!source) {
            m_toolbar.ShowStatusMessage("No entity selected to duplicate", 1.2f);
            return false;
        }

        if (m_undoRedo.HasPendingSnapshot()) {
            m_undoRedo.CommitContinuousEdit(scene);
        }

        m_gui.CancelSliderDrag();
        m_gui.UnfocusTextField();
        m_viewport.CancelEntityDrag();

        std::string sourceName = source->name;
        m_undoRedo.PushUndoState(
            scene,
            m_selectedEntityId,
            m_gui.GetTextCursorPos(),
            m_gui.GetTextSelectionAnchor(),
            m_isPlaying
        );

        Scene::Entity* duplicated = scene.DuplicateEntity(m_selectedEntityId);
        if (!duplicated) {
            return false;
        }

        m_selectedEntityId = duplicated->id;
        m_toolbar.ShowStatusMessage("Duplicated -> '" + duplicated->name + "' [Ctrl+D]", 1.5f);
        TALOS_LOG_INFO("Duplicated entity '" + sourceName + "' -> '" + duplicated->name + "' (id=" + std::to_string(duplicated->id) + ").");
        return true;
    }

    // =========================================================================
    // Multi-Scene Navigation & Creation
    // =========================================================================

    bool Editor::OpenScene(Scene::Scene& scene, const std::string& filePath, bool autoSavePrevious) {
        std::string normalizedTarget = Scene::SceneSerializer::NormalizeScenePath(filePath);
        if (normalizedTarget.empty()) return false;

        if (!m_isPlaying && autoSavePrevious && !m_sceneFilePath.empty()) {
            std::string normalizedCurrent = Scene::SceneSerializer::NormalizeScenePath(m_sceneFilePath);
            if (normalizedCurrent != normalizedTarget) {
                Scene::SceneSerializer::Save(scene, normalizedCurrent);
            }
        }

        m_gui.CancelSliderDrag();
        m_gui.UnfocusTextField();
        m_viewport.CancelEntityDrag();

        if (!scene.ChangeScene(normalizedTarget, false)) {
            m_toolbar.ShowStatusMessage("Failed to open scene '" + normalizedTarget + "'", 2.0f);
            return false;
        }

        m_sceneFilePath = scene.GetSceneFilePath();
        m_selectedEntityId = scene.GetEntities().empty() ? 0 : scene.GetEntities().front().id;
        if (!m_isPlaying) {
            m_undoRedo.Clear();
        }
        m_toolbar.ShowStatusMessage(
            std::string(m_isPlaying ? "Switched live to scene '" : "Opened scene '") + m_sceneFilePath + "'",
            1.8f
        );
        return true;
    }

    bool Editor::CreateNewScene(Scene::Scene& scene, const std::string& baseName) {
        if (m_isPlaying) return false;

        if (m_undoRedo.HasPendingSnapshot()) {
            m_undoRedo.CommitContinuousEdit(scene);
        }
        if (!m_sceneFilePath.empty()) {
            Scene::SceneSerializer::Save(scene, m_sceneFilePath);
        }

        m_gui.CancelSliderDrag();
        m_gui.UnfocusTextField();
        m_viewport.CancelEntityDrag();

        std::string newPath = Scene::SceneSerializer::BuildUniqueScenePath(baseName);
        scene.Clear();
        scene.SetSceneFilePath(newPath);

        Scene::Entity& cam = scene.CreateEntity("Main Camera");
        cam.tag = "Camera";
        cam.transform.x = 640.0f;
        cam.transform.y = 360.0f;
        cam.camera.enabled = true;
        cam.camera.isPrimary = true;

        if (!Scene::SceneSerializer::Save(scene, newPath)) {
            m_toolbar.ShowStatusMessage("Failed to create scene '" + newPath + "'", 2.0f);
            return false;
        }

        m_sceneFilePath = newPath;
        m_selectedEntityId = cam.id;
        m_undoRedo.Clear();
        m_assetBrowserDock.Refresh();
        m_toolbar.ShowStatusMessage("Created new scene -> '" + newPath + "' [Ctrl+N]", 2.0f);
        TALOS_LOG_INFO("Created and opened new scene '" + newPath + "'.");
        return true;
    }

    bool Editor::CycleScene(Scene::Scene& scene, int direction) {
        if (direction == 0) return false;
        std::vector<std::string> scenes = Scene::SceneSerializer::DiscoverScenes("examples/scenes");
        std::string currentNorm = Scene::SceneSerializer::NormalizeScenePath(m_sceneFilePath);
        if (!currentNorm.empty()) {
            bool found = false;
            for (const auto& sp : scenes) {
                if (sp == currentNorm) { found = true; break; }
            }
            if (!found) scenes.push_back(currentNorm);
        }
        if (scenes.size() <= 1) {
            m_toolbar.ShowStatusMessage("Only 1 scene found in examples/scenes", 1.5f);
            return false;
        }

        int currentIdx = 0;
        for (int i = 0; i < static_cast<int>(scenes.size()); ++i) {
            if (scenes[i] == currentNorm) {
                currentIdx = i;
                break;
            }
        }
        int total = static_cast<int>(scenes.size());
        int nextIdx = (currentIdx + (direction > 0 ? 1 : -1) + total) % total;
        return OpenScene(scene, scenes[nextIdx], !m_isPlaying);
    }

    // =========================================================================
    // Play / Stop Simulation Mode Lifecycle
    // =========================================================================

    void Editor::StartPlayMode(Scene::Scene& scene) {
        if (m_isPlaying) return;

        if (m_undoRedo.HasPendingSnapshot()) {
            m_undoRedo.CommitContinuousEdit(scene);
        }

        scene.ResetTriggerState();
        scene.CancelPendingSceneChange();
        m_prePlaySceneFilePath = m_sceneFilePath;
        scene.SetSceneFilePath(m_sceneFilePath);
        m_editSnapshot = scene.GetEntities();
        m_isPlaying = true;
        m_gui.CancelSliderDrag();
        m_gui.UnfocusTextField();
        m_viewport.CancelEntityDrag();
        m_projectDialog.Close(m_gui);
        m_inputMapDialog.Close(m_gui);
        m_toolbar.CloseAudioMixer(m_gui);

        m_toolbar.ShowStatusMessage("Simulation Started (Play Mode)", 2.0f);
        TALOS_LOG_INFO("Entering Play Mode (snapshot saved: " + std::to_string(m_editSnapshot.size()) + " entities)");
    }

    void Editor::StopPlayMode(Scene::Scene& scene) {
        if (!m_isPlaying) return;

        Audio::AudioSystem::StopAllSounds();
        Audio::AudioSystem::StopAllMusic();
        scene.ResetTriggerState();
        scene.CancelPendingSceneChange();
        if (!m_prePlaySceneFilePath.empty()) {
            m_sceneFilePath = m_prePlaySceneFilePath;
            scene.SetSceneFilePath(m_prePlaySceneFilePath);
        }
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
        m_projectDialog.BindTextFields(m_gui);
        m_sceneTreeDock.BindTextFields(m_gui);

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

        if (m_toolbar.IsAudioMixerOpen() && Core::Input::IsKeyPressedOnce(Core::Key::Escape)) {
            m_toolbar.CloseAudioMixer(m_gui);
            return;
        }

        bool ctrlDown = Core::Input::IsKeyDown(Core::Key::LeftControl) ||
                        Core::Input::IsKeyDown(Core::Key::RightControl);
        bool shiftDown = Core::Input::IsKeyDown(Core::Key::LeftShift) ||
                         Core::Input::IsKeyDown(Core::Key::RightShift);
        bool altDown = Core::Input::IsKeyDown(Core::Key::LeftAlt) ||
                       Core::Input::IsKeyDown(Core::Key::RightAlt);

        // In Play Mode, advance the live game simulation (Movement + Collisions)
        // and allow [F3] to toggle the Diagnostic Overlay or [Alt+Left/Right] to cycle scenes.
        if (m_isPlaying) {
            if (Core::Input::IsKeyPressedOnce(Core::Key::F3)) {
                bool nowEnabled = m_diagnosticOverlay.Toggle();
                m_toolbar.ShowStatusMessage(
                    nowEnabled ? "Diagnostic Overlay Enabled [F3]" : "Diagnostic Overlay Disabled [F3]",
                    1.5f
                );
            }
            if (altDown && !ctrlDown) {
                if (Core::Input::IsKeyPressedOnce(Core::Key::Left)) {
                    CycleScene(scene, -1);
                } else if (Core::Input::IsKeyPressedOnce(Core::Key::Right)) {
                    CycleScene(scene, 1);
                }
            }

            std::string beforePath = scene.GetSceneFilePath();
            scene.Update(delta);
            if (!scene.GetSceneFilePath().empty() && scene.GetSceneFilePath() != beforePath) {
                m_sceneFilePath = scene.GetSceneFilePath();
                if (m_selectedEntityId != 0 && !scene.FindEntityById(m_selectedEntityId)) {
                    m_selectedEntityId = scene.GetEntities().empty() ? 0 : scene.GetEntities().front().id;
                }
                m_toolbar.ShowStatusMessage("Runtime switched scene -> '" + m_sceneFilePath + "'", 2.0f);
            }
            return;
        }

        // Advance active sprite frame animations in Edit Mode for live preview
        // (paused while the user is actively dragging a slider or viewport gizmo).
        if (!m_undoRedo.HasPendingSnapshot()) {
            scene.UpdateAnimations(delta);
        }

        // Universal Undo / Redo, Duplicate Entity, New Scene & Save Project keyboard shortcuts
        // (Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z / Ctrl+D / Ctrl+N / Ctrl+S)
        if (ctrlDown && !m_inputMapDialog.IsListeningForKey()) {
            while (Core::Input::GetCharPressed() > 0) {}

            if (Core::Input::IsKeyPressedOnce(Core::Key::S)) {
                std::string saveStatus;
                m_projectDialog.SaveProject(scene, m_sceneFilePath, saveStatus);
                m_toolbar.ShowStatusMessage(saveStatus, 2.0f);
                return;
            }
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
            if (Core::Input::IsKeyPressedOnce(Core::Key::D) &&
                !m_projectDialog.IsOpen() &&
                !m_inputMapDialog.IsOpen() &&
                !m_toolbar.IsAudioMixerOpen()) {
                DuplicateSelectedEntity(scene);
                return;
            }
            if (Core::Input::IsKeyPressedOnce(Core::Key::N) &&
                !m_projectDialog.IsOpen() &&
                !m_inputMapDialog.IsOpen() &&
                !m_toolbar.IsAudioMixerOpen()) {
                CreateNewScene(scene);
                return;
            }
        }

        if (m_projectDialog.IsOpen() &&
            Core::Input::IsKeyPressedOnce(Core::Key::Escape) &&
            m_gui.GetFocusedTextFieldId() == -1) {
            m_projectDialog.Close(m_gui);
            return;
        }

        // Modal Input Map interactive key listening takes priority over normal keyboard shortcuts
        if (m_inputMapDialog.IsOpen()) {
            std::string modalStatus;
            if (m_inputMapDialog.UpdateKeyListening(modalStatus)) {
                if (!modalStatus.empty()) {
                    m_projectDialog.SyncSubsystemsToProjectFile();
                    m_toolbar.ShowStatusMessage(modalStatus, 2.0f);
                }
                return;
            }
            if (Core::Input::IsKeyPressedOnce(Core::Key::Escape) && m_gui.GetFocusedTextFieldId() == -1) {
                m_inputMapDialog.Close(m_gui);
                return;
            }
        }

        // Process keyboard typing for any focused text field (Inspector, Project dialog, or Input Map modal)
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
                m_projectDialog.SyncSubsystemsToProjectFile();
                m_toolbar.ShowStatusMessage(addActionStatus, 2.0f);
            }
        }

        // Block 2D viewport navigation and entity picking while a modal dialog is open
        if (m_projectDialog.IsOpen() || m_inputMapDialog.IsOpen() || m_toolbar.IsAudioMixerOpen()) {
            return;
        }

        // Multi-scene cycling via Alt+Left / Alt+Right when no text field is focused
        if (altDown && !ctrlDown && m_gui.GetFocusedTextFieldId() == -1) {
            if (Core::Input::IsKeyPressedOnce(Core::Key::Left)) {
                CycleScene(scene, -1);
                return;
            }
            if (Core::Input::IsKeyPressedOnce(Core::Key::Right)) {
                CycleScene(scene, 1);
                return;
            }
        }

        // Hierarchy reordering via Alt+Up / Alt+Down when an entity is selected
        if (altDown && !ctrlDown && m_selectedEntityId != 0 && m_gui.GetFocusedTextFieldId() == -1) {
            int selIdx = scene.GetEntityIndex(m_selectedEntityId);
            if (Core::Input::IsKeyPressedOnce(Core::Key::Up) && selIdx > 0) {
                m_undoRedo.PushUndoState(scene, m_selectedEntityId,
                                         m_gui.GetTextCursorPos(), m_gui.GetTextSelectionAnchor(), m_isPlaying);
                if (scene.MoveEntityUp(m_selectedEntityId)) {
                    const Scene::Entity* moved = scene.FindEntityById(m_selectedEntityId);
                    m_toolbar.ShowStatusMessage("Moved '" + (moved ? moved->name : std::string("Entity")) + "' up in hierarchy [Alt+Up]", 1.5f);
                }
                return;
            }
            if (Core::Input::IsKeyPressedOnce(Core::Key::Down) &&
                selIdx >= 0 && selIdx + 1 < static_cast<int>(scene.GetEntities().size())) {
                m_undoRedo.PushUndoState(scene, m_selectedEntityId,
                                         m_gui.GetTextCursorPos(), m_gui.GetTextSelectionAnchor(), m_isPlaying);
                if (scene.MoveEntityDown(m_selectedEntityId)) {
                    const Scene::Entity* moved = scene.FindEntityById(m_selectedEntityId);
                    m_toolbar.ShowStatusMessage("Moved '" + (moved ? moved->name : std::string("Entity")) + "' down in hierarchy [Alt+Down]", 1.5f);
                }
                return;
            }
        }

        // 2D Canvas Viewport shortcuts & mouse interaction in Edit Mode
        float consoleH = m_consoleDock.GetPanelHeight();
        if (m_gui.GetFocusedTextFieldId() == -1 && !ctrlDown && !altDown) {
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
            } else if (Core::Input::IsKeyPressedOnce(Core::Key::G)) {
                m_toolbar.ShowStatusMessage(
                    m_viewport.ToggleSnapToGrid(),
                    1.4f
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
        m_projectDialog.BindTextFields(m_gui);
        m_sceneTreeDock.BindTextFields(m_gui);
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
                         m_projectDialog,
                         m_inputMapDialog,
                         requestStartPlay,
                         requestStopPlay);

        if (requestStartPlay) StartPlayMode(scene);
        if (requestStopPlay)  StopPlayMode(scene);

        std::string sceneTreeStatus;
        m_sceneTreeDock.Render(scene,
                               m_windowHeight,
                               consoleH,
                               m_sceneFilePath,
                               m_selectedEntityId,
                               m_isPlaying,
                               m_gui,
                               m_undoRedo,
                               &sceneTreeStatus);
        if (!sceneTreeStatus.empty()) {
            m_toolbar.ShowStatusMessage(sceneTreeStatus, 1.6f);
        }

        std::string inspectorStatus;
        m_inspectorDock.Render(scene, m_windowWidth, m_windowHeight, m_selectedEntityId, m_gui,
                               &m_assetBrowserDock, &inspectorStatus);
        if (!inspectorStatus.empty()) {
            m_toolbar.ShowStatusMessage(inspectorStatus, 2.0f);
        }

        std::string bottomStatus;
        m_consoleDock.Render(m_windowWidth,
                             m_windowHeight,
                             scene,
                             m_selectedEntityId,
                             m_sceneFilePath,
                             m_isPlaying,
                             m_gui,
                             m_undoRedo,
                             m_assetBrowserDock,
                             bottomStatus);
        if (!bottomStatus.empty()) {
            m_toolbar.ShowStatusMessage(bottomStatus, 2.0f);
        }

        // 4. Project Modal Dialogs (rendered on top of docks and viewport)
        if (m_projectDialog.IsOpen()) {
            bool requestOpenInputMap = false;
            bool requestOpenAudioMixer = false;
            std::string projStatus;
            m_projectDialog.Render(scene,
                                   m_windowWidth,
                                   m_windowHeight,
                                   consoleH,
                                   m_sceneFilePath,
                                   m_selectedEntityId,
                                   m_gui,
                                   m_undoRedo,
                                   requestOpenInputMap,
                                   requestOpenAudioMixer,
                                   projStatus);
            if (!projStatus.empty()) {
                m_toolbar.ShowStatusMessage(projStatus, 2.2f);
            }
            if (requestOpenInputMap) {
                m_projectDialog.Close(m_gui);
                m_toolbar.CloseAudioMixer(m_gui);
                if (!m_inputMapDialog.IsOpen()) {
                    m_inputMapDialog.Toggle(m_gui);
                }
            } else if (requestOpenAudioMixer) {
                m_projectDialog.Close(m_gui);
                if (m_inputMapDialog.IsOpen()) {
                    m_inputMapDialog.Close(m_gui);
                }
                m_toolbar.OpenAudioMixer(m_gui);
            }
        }
        if (m_inputMapDialog.IsOpen()) {
            std::string modalStatus;
            m_inputMapDialog.Render(m_windowWidth, m_windowHeight, consoleH, m_gui, modalStatus);
            if (!modalStatus.empty()) {
                m_projectDialog.SyncSubsystemsToProjectFile();
                m_toolbar.ShowStatusMessage(modalStatus, 2.0f);
            }
        }
        if (m_toolbar.IsAudioMixerOpen()) {
            m_toolbar.RenderAudioMixerModal(m_windowWidth, m_windowHeight, consoleH, m_gui, m_projectDialog);
        }
    }

}
