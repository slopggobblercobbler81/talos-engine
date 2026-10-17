#include "editor/toolbar/editor_toolbar.h"
#include "scene/scene_serializer.h"
#include "core/logger.h"
#include <cstdio>

namespace Talos::Editor {

    using namespace Talos::Renderer;
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
        float centerBtnX = (w * 0.5f) - 210.0f;
        if (!isPlaying) {
            if (gui.DrawButton(centerBtnX, 8.0f, 120.0f, 30.0f, "PLAY GAME",
                               { 40, 155, 85, 255 }, { 55, 185, 105, 255 })) {
                outRequestStartPlay = true;
            }

            if (m_statusTimer > 0.0f) {
                Renderer2D::DrawLabel(m_statusMessage.c_str(), centerBtnX + 132.0f, 15.0f, 16, { 95, 230, 135, 255 });
            } else {
                Renderer2D::DrawLabel("MODE: EDITOR", centerBtnX + 132.0f, 15.0f, 16, { 160, 165, 180, 255 });
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

        // Entity Management, Input Map & Scene Serialization Buttons (only active in Edit Mode)
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

            // Right-side Input Map, Scene Save & Load buttons
            float rightActionsX = w - RIGHT_PANEL_WIDTH - 228.0f;
            bool mapOpen = inputMapDialog.IsOpen();
            if (gui.DrawButton(rightActionsX, 8.0f, 104.0f, 30.0f, "Input Map",
                               mapOpen ? ColorRGBA{ 55, 130, 220, 255 } : ColorRGBA{ 92, 68, 160, 255 },
                               mapOpen ? ColorRGBA{ 75, 155, 245, 255 } : ColorRGBA{ 115, 88, 192, 255 })) {
                inputMapDialog.Toggle(gui);
            }

            if (gui.DrawButton(rightActionsX + 112.0f, 8.0f, 104.0f, 30.0f, "Save Scene",
                               { 48, 125, 115, 255 }, { 65, 155, 142, 255 })) {
                if (SceneSerializer::Save(scene, sceneFilePath)) {
                    ShowStatusMessage("Saved -> " + sceneFilePath);
                } else {
                    ShowStatusMessage("Save failed!");
                }
            }

            if (gui.DrawButton(rightActionsX + 224.0f, 8.0f, 104.0f, 30.0f, "Load Scene",
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

}
