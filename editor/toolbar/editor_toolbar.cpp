#include "editor/toolbar/editor_toolbar.h"
#include "audio/audio_system.h"
#include "scene/scene_serializer.h"
#include "core/logger.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Talos::Editor {

    using namespace Talos::Renderer;
    using namespace Talos::Audio;
    using namespace Talos::Scene;
    using namespace Talos::Editor::EditorLayout;

    void EditorToolbar::ShowStatusMessage(const std::string& message, float durationSeconds) {
        if (message.empty()) return;
        m_statusMessage = message;
        m_statusTimer   = durationSeconds;
    }

    void EditorToolbar::UpdateTimer(float delta) {
        if (m_statusTimer > 0.0f) {
            m_statusTimer -= delta;
            if (m_statusTimer < 0.0f) {
                m_statusTimer = 0.0f;
            }
        }
    }

    void EditorToolbar::OpenAudioMixer(EditorGui& gui) {
        m_audioMixerOpen = true;
        gui.CancelSliderDrag();
        gui.UnfocusTextField();
    }

    void EditorToolbar::CloseAudioMixer(EditorGui& gui) {
        if (m_audioMixerOpen) {
            m_audioMixerOpen = false;
            gui.CancelSliderDrag();
        }
    }

    void EditorToolbar::Render(Scene::Scene& scene,
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
                               ProjectDialog& projectDialog,
                               InputMapDialog& inputMapDialog,
                               bool& outRequestStartPlay,
                               bool& outRequestStopPlay) {
        outRequestStartPlay = false;
        outRequestStopPlay  = false;

        float w = static_cast<float>(windowWidth);

        // Toolbar background and bottom divider line
        Renderer2D::DrawRect(0.0f, 0.0f, w, TOOLBAR_HEIGHT, { 20, 21, 26, 255 });
        Renderer2D::DrawLine2D(0.0f, TOOLBAR_HEIGHT, w, TOOLBAR_HEIGHT, { 55, 58, 70, 255 });

        // Engine Brand Title
        Renderer2D::DrawLabel("TALOS ENGINE", 16.0f, 13.0f, 20, { 95, 195, 255, 255 });
        Renderer2D::DrawLabel("v0.1", 168.0f, 16.0f, 16, { 130, 135, 150, 255 });

        // Instant Play / Stop Simulation Toggle Button
        float centerBtnX = (w * 0.5f) - 196.0f;
        if (!isPlaying) {
            if (gui.DrawButton(centerBtnX, 8.0f, 120.0f, 30.0f, "PLAY GAME",
                               { 40, 155, 85, 255 }, { 55, 185, 105, 255 })) {
                outRequestStartPlay = true;
            }

            if (m_statusTimer > 0.0f) {
                Renderer2D::DrawLabel(m_statusMessage.c_str(), centerBtnX + 132.0f, 15.0f, 16, { 95, 230, 135, 255 });
            } else {
                std::string modeBanner = "PROJECT: " + projectDialog.GetConfig().name;
                if (modeBanner.size() > 26) {
                    modeBanner = modeBanner.substr(0, 23) + "...";
                }
                Renderer2D::DrawLabel(modeBanner.c_str(), centerBtnX + 132.0f, 15.0f, 15, { 160, 165, 180, 255 });
            }
        } else {
            if (gui.DrawButton(centerBtnX, 8.0f, 120.0f, 30.0f, "STOP GAME",
                               { 210, 65, 65, 255 }, { 235, 85, 85, 255 })) {
                outRequestStopPlay = true;
            }
            Renderer2D::DrawLabel("MODE: PLAYING LIVE", centerBtnX + 132.0f, 15.0f, 16, { 95, 230, 135, 255 });
            Renderer2D::DrawLabel(
                diagnosticOverlayEnabled ? "[F3] Diagnostics: ON" : "[F3] Diagnostics: OFF",
                centerBtnX + 296.0f, 15.0f, 15,
                diagnosticOverlayEnabled ? ColorRGBA{ 95, 205, 255, 255 } : ColorRGBA{ 130, 138, 158, 255 }
            );
        }

        // Right-side command buttons
        float rightActionsX = w - 514.0f;

        // Audio Mixer button (available in both Edit and Play Mode)
        if (gui.DrawButton(rightActionsX + 96.0f, 8.0f, 94.0f, 30.0f, "Audio Mix",
                           m_audioMixerOpen ? ColorRGBA{ 55, 130, 220, 255 } : ColorRGBA{ 48, 95, 145, 255 },
                           m_audioMixerOpen ? ColorRGBA{ 75, 155, 245, 255 } : ColorRGBA{ 65, 120, 180, 255 })) {
            m_audioMixerOpen = !m_audioMixerOpen;
            if (m_audioMixerOpen) {
                if (inputMapDialog.IsOpen()) inputMapDialog.Close(gui);
                if (projectDialog.IsOpen())  projectDialog.Close(gui);
            }
            gui.CancelSliderDrag();
        }

        // Entity Management, Project Workflow, Input Map & Scene Serialization Buttons (only active in Edit Mode)
        if (!isPlaying) {
            if (gui.DrawButton(LEFT_PANEL_WIDTH + 12.0f, 8.0f, 115.0f, 30.0f, "+ New Entity",
                               { 55, 115, 200, 255 }, { 75, 140, 230, 255 })) {
                undoRedo.PushUndoState(
                    scene,
                    inOutSelectedEntityId,
                    gui.GetTextCursorPos(),
                    gui.GetTextSelectionAnchor(),
                    isPlaying
                );
                char nameBuf[32];
                std::snprintf(nameBuf, sizeof(nameBuf), "Object_%d", m_newEntityCounter++);

                float vpCenterX = LEFT_PANEL_WIDTH + (w - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH) * 0.5f;
                float vpCenterY = TOOLBAR_HEIGHT + (static_cast<float>(windowHeight) - TOOLBAR_HEIGHT - consoleHeight) * 0.5f;
                float spawnWorldX = 480.0f, spawnWorldY = 320.0f;
                camera.ScreenToWorld(vpCenterX, vpCenterY, spawnWorldX, spawnWorldY);

                Entity& created = scene.CreateEntity(nameBuf, spawnWorldX - 32.0f, spawnWorldY - 32.0f);
                created.sprite.color = EditorGui::PRESET_COLORS[m_newEntityCounter % EditorGui::NUM_PRESET_COLORS];
                inOutSelectedEntityId = created.id;
                TALOS_LOG_INFO("Created new entity '" + created.name + "' (id=" + std::to_string(created.id) + ").");
            }

            if (inOutSelectedEntityId != 0) {
                if (gui.DrawButton(LEFT_PANEL_WIDTH + 136.0f, 8.0f, 85.0f, 30.0f, "Delete",
                                   { 140, 50, 55, 255 }, { 180, 65, 70, 255 })) {
                    undoRedo.PushUndoState(
                        scene,
                        inOutSelectedEntityId,
                        gui.GetTextCursorPos(),
                        gui.GetTextSelectionAnchor(),
                        isPlaying
                    );
                    const Entity* target = scene.FindEntityById(inOutSelectedEntityId);
                    std::string deletedName = target ? target->name : std::to_string(inOutSelectedEntityId);
                    scene.RemoveEntity(inOutSelectedEntityId);
                    inOutSelectedEntityId = 0;
                    TALOS_LOG_INFO("Deleted entity '" + deletedName + "'.");
                }
            }

            bool projOpen = projectDialog.IsOpen();
            if (gui.DrawButton(rightActionsX, 8.0f, 88.0f, 30.0f, "Project",
                               projOpen ? ColorRGBA{ 55, 130, 220, 255 } : ColorRGBA{ 42, 112, 152, 255 },
                               projOpen ? ColorRGBA{ 75, 155, 245, 255 } : ColorRGBA{ 58, 138, 185, 255 })) {
                projectDialog.Toggle(gui);
                if (projectDialog.IsOpen()) {
                    m_audioMixerOpen = false;
                    if (inputMapDialog.IsOpen()) {
                        inputMapDialog.Close(gui);
                    }
                }
            }

            bool mapOpen = inputMapDialog.IsOpen();
            if (gui.DrawButton(rightActionsX + 198.0f, 8.0f, 96.0f, 30.0f, "Input Map",
                               mapOpen ? ColorRGBA{ 55, 130, 220, 255 } : ColorRGBA{ 92, 68, 160, 255 },
                               mapOpen ? ColorRGBA{ 75, 155, 245, 255 } : ColorRGBA{ 115, 88, 192, 255 })) {
                inputMapDialog.Toggle(gui);
                if (inputMapDialog.IsOpen()) {
                    m_audioMixerOpen = false;
                    if (projectDialog.IsOpen()) {
                        projectDialog.Close(gui);
                    }
                }
            }

            if (gui.DrawButton(rightActionsX + 302.0f, 8.0f, 96.0f, 30.0f, "Save Scene",
                               { 48, 125, 115, 255 }, { 65, 155, 142, 255 })) {
                if (SceneSerializer::Save(scene, sceneFilePath)) {
                    projectDialog.SyncSubsystemsToProjectFile();
                    ShowStatusMessage("Saved -> " + sceneFilePath);
                } else {
                    ShowStatusMessage("Save failed!");
                }
            }

            if (gui.DrawButton(rightActionsX + 406.0f, 8.0f, 96.0f, 30.0f, "Load Scene",
                               { 70, 75, 95, 255 }, { 92, 98, 122, 255 })) {
                undoRedo.PushUndoState(
                    scene,
                    inOutSelectedEntityId,
                    gui.GetTextCursorPos(),
                    gui.GetTextSelectionAnchor(),
                    isPlaying
                );
                if (SceneSerializer::Load(scene, sceneFilePath)) {
                    inOutSelectedEntityId = scene.GetEntities().empty() ? 0 : scene.GetEntities().front().id;
                    ShowStatusMessage("Loaded <- " + sceneFilePath);
                } else {
                    ShowStatusMessage("No saved scene found!");
                }
            }
        }
    }

    void EditorToolbar::RenderAudioMixerModal(int windowWidth,
                                              int windowHeight,
                                              float consoleHeight,
                                              EditorGui& gui,
                                              ProjectDialog& projectDialog) {
        if (!m_audioMixerOpen) return;

        float w = static_cast<float>(windowWidth);
        float h = static_cast<float>(windowHeight);
        Renderer2D::DrawRect(0.0f, TOOLBAR_HEIGHT, w, h - TOOLBAR_HEIGHT, { 8, 9, 14, 175 });

        float modalW = 460.0f;
        float modalH = 232.0f;
        float vpCenterX = LEFT_PANEL_WIDTH + (w - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH) * 0.5f;
        float vpCenterY = TOOLBAR_HEIGHT + (h - TOOLBAR_HEIGHT - consoleHeight) * 0.5f;
        float mx = std::round(vpCenterX - modalW * 0.5f);
        float my = std::round(vpCenterY - modalH * 0.5f);

        Renderer2D::DrawRect(mx, my, modalW, modalH, { 24, 26, 34, 252 });
        Renderer2D::DrawRectOutline(mx, my, modalW, modalH, 2.0f, { 75, 145, 225, 255 });
        Renderer2D::DrawRect(mx + 2.0f, my + 2.0f, modalW - 4.0f, 38.0f, { 32, 36, 48, 255 });

        Renderer2D::DrawLabel("PROJECT AUDIO VOLUME MIXER", mx + 16.0f, my + 12.0f, 16, { 110, 200, 255, 255 });

        if (gui.DrawButton(mx + modalW - 146.0f, my + 8.0f, 64.0f, 24.0f, "Reset",
                           { 48, 85, 145, 255 }, { 64, 112, 188, 255 })) {
            AudioSystem::ResetDefaultVolumes();
            AudioSystem::SaveConfig();
            projectDialog.SyncSubsystemsToProjectFile();
            ShowStatusMessage("Audio buses reset to 100%", 1.5f);
        }
        if (gui.DrawButton(mx + modalW - 74.0f, my + 8.0f, 60.0f, 24.0f, "Close",
                           { 130, 50, 55, 255 }, { 168, 65, 72, 255 })) {
            CloseAudioMixer(gui);
            return;
        }

        float cx = mx + 18.0f;
        float cw = modalW - 36.0f;
        float cy = my + 50.0f;

        std::string configBanner = "Project File: " + projectDialog.GetProjectFilePath() + " ([Audio])";
        Renderer2D::DrawLabel(configBanner.c_str(), cx, cy, 13, { 145, 152, 170, 255 });
        cy += 24.0f;

        float prevMaster = AudioSystem::GetMasterVolume();
        float prevSfx    = AudioSystem::GetSFXVolume();
        float prevMusic  = AudioSystem::GetMusicBusVolume();

        auto drawBusRow = [&](const char* label, float currentVal, int sliderId) -> float {
            float pct = std::round(currentVal * 100.0f);
            gui.DrawFloatSlider(cx, cy, cw - 88.0f, label, pct, 0.0f, 100.0f, sliderId);
            float updated = std::max(0.0f, std::min(1.0f, std::round(pct) / 100.0f));
            bool muted = (updated < 0.01f);
            bool full  = (std::fabs(updated - 1.0f) < 0.01f);
            if (gui.DrawButton(cx + cw - 82.0f, cy, 38.0f, 17.0f, "Mute",
                               muted ? ColorRGBA{ 155, 55, 60, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                               { 185, 70, 75, 255 })) {
                updated = 0.0f;
            }
            if (gui.DrawButton(cx + cw - 40.0f, cy, 40.0f, 17.0f, "100%",
                               full ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                               { 68, 135, 220, 255 })) {
                updated = 1.0f;
            }
            cy += 26.0f;
            return updated;
        };

        float newMaster = drawBusRow("Master (%)", prevMaster, 120);
        float newSfx    = drawBusRow("SFX (%)",    prevSfx,    121);
        float newMusic  = drawBusRow("Music (%)",  prevMusic,  122);

        if (std::fabs(newMaster - prevMaster) > 1e-4f ||
            std::fabs(newSfx - prevSfx) > 1e-4f ||
            std::fabs(newMusic - prevMusic) > 1e-4f) {
            AudioSystem::SetMasterVolume(newMaster);
            AudioSystem::SetSFXVolume(newSfx);
            AudioSystem::SetMusicBusVolume(newMusic);
            AudioSystem::SaveConfig();
            projectDialog.SyncSubsystemsToProjectFile();
        }

        cy += 6.0f;
        Renderer2D::DrawLine2D(cx, cy, cx + cw, cy, { 46, 50, 62, 255 });
        cy += 10.0f;

        if (gui.DrawButton(cx, cy, 132.0f, 26.0f, "Test SFX (Coin)",
                           { 42, 120, 78, 255 }, { 56, 152, 98, 255 })) {
            AudioSystem::PlaySound("examples/assets/audio/coin.wav", 1.0f, 1.0f, 0.5f);
        }
        if (gui.DrawButton(cx + 142.0f, cy, 132.0f, 26.0f, "Test Music",
                           { 48, 105, 175, 255 }, { 64, 132, 215, 255 })) {
            AudioSystem::PlayMusic("examples/assets/audio/music_loop.wav", true, 1.0f, 1.0f, 0.5f);
        }
        if (gui.DrawButton(cx + 284.0f, cy, cw - 284.0f, 26.0f, "Stop Preview",
                           { 130, 50, 55, 255 }, { 168, 65, 72, 255 })) {
            AudioSystem::StopAllSounds();
            AudioSystem::StopAllMusic();
        }
    }

}
