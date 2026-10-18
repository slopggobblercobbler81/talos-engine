#include "editor/viewport/canvas_viewport.h"
#include "core/input.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Scene;
    using namespace Talos::Editor::EditorLayout;

    void CanvasViewport::OnMouseReleased(bool leftReleased, bool middleOrRightReleased) {
        if (leftReleased) {
            m_isDraggingEntity = false;
            if (!Input::IsMouseButtonDown(MouseButton::Middle) && !Input::IsMouseButtonDown(MouseButton::Right)) {
                m_isPanningCamera = false;
            }
        }
        if (middleOrRightReleased) {
            if (!Input::IsMouseButtonDown(MouseButton::Middle) && !Input::IsMouseButtonDown(MouseButton::Right)) {
                m_isPanningCamera = false;
            }
        }
    }

    std::string CanvasViewport::ResetCamera() {
        m_camera.targetX = 0.0f;
        m_camera.targetY = 0.0f;
        m_camera.offsetX = 0.0f;
        m_camera.offsetY = 0.0f;
        m_camera.zoom    = 1.0f;
        return "Camera Reset (100%)";
    }

    std::string CanvasViewport::FocusSelectedEntity(const Scene::Scene& scene,
                                                    Scene::EntityID selectedEntityId,
                                                    int windowWidth,
                                                    int windowHeight,
                                                    float consoleHeight) {
        const Entity* selected = const_cast<Scene::Scene&>(scene).FindEntityById(selectedEntityId);
        if (!selected) {
            return ResetCamera();
        }

        float viewportX = LEFT_PANEL_WIDTH;
        float viewportY = TOOLBAR_HEIGHT;
        float viewportW = static_cast<float>(windowWidth) - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
        float viewportH = static_cast<float>(windowHeight) - TOOLBAR_HEIGHT - consoleHeight;

        float viewportCenterX = viewportX + viewportW * 0.5f;
        float viewportCenterY = viewportY + viewportH * 0.5f;

        float entityCenterX = selected->transform.x + selected->transform.width * 0.5f;
        float entityCenterY = selected->transform.y + selected->transform.height * 0.5f;

        m_camera.offsetX = 0.0f;
        m_camera.offsetY = 0.0f;
        m_camera.targetX = entityCenterX - (viewportCenterX / m_camera.zoom);
        m_camera.targetY = entityCenterY - (viewportCenterY / m_camera.zoom);
        return "Focused on " + selected->name;
    }

    void CanvasViewport::HandleInput(Scene::Scene& scene,
                                     int windowWidth,
                                     int windowHeight,
                                     float consoleHeight,
                                     Scene::EntityID& inOutSelectedEntityId,
                                     EditorGui& gui,
                                     EditorUndoRedo& undoRedo,
                                     bool isPlaying) {
        float viewportX = LEFT_PANEL_WIDTH;
        float viewportY = TOOLBAR_HEIGHT;
        float viewportW = static_cast<float>(windowWidth) - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
        float viewportH = static_cast<float>(windowHeight) - TOOLBAR_HEIGHT - consoleHeight;

        float mx = Input::GetMouseX();
        float my = Input::GetMouseY();
        bool mouseInViewport = EditorGui::IsMouseOver(viewportX, viewportY, viewportW, viewportH);

        // Protect the bottom-left Camera Overlay HUD bar from triggering viewport entity clicks
        float hudX = viewportX + 12.0f;
        float hudY = static_cast<float>(windowHeight) - consoleHeight - 38.0f;
        float hudW = 240.0f;
        float hudH = 28.0f;
        bool mouseOverCameraHud = EditorGui::IsMouseOver(hudX, hudY, hudW, hudH);

        // =====================================================================
        // 1. Interactive 2D Camera Zoom (Mouse Wheel, Zoom-to-Cursor)
        // =====================================================================
        if (mouseInViewport) {
            float wheel = Input::GetMouseWheelMove();
            if (std::fabs(wheel) > 0.001f) {
                float worldBeforeX = 0.0f;
                float worldBeforeY = 0.0f;
                m_camera.ScreenToWorld(mx, my, worldBeforeX, worldBeforeY);

                float zoomFactor = (wheel > 0.0f) ? 1.12f : (1.0f / 1.12f);
                m_camera.zoom = std::max(0.20f, std::min(5.00f, m_camera.zoom * zoomFactor));

                m_camera.targetX = worldBeforeX - (mx - m_camera.offsetX) / m_camera.zoom;
                m_camera.targetY = worldBeforeY - (my - m_camera.offsetY) / m_camera.zoom;
            }
        }

        // =====================================================================
        // 2. Interactive 2D Camera Pan (Middle-Click, Right-Click, or Space+Left-Click)
        // =====================================================================
        bool spacePanInitiated = (gui.GetFocusedTextFieldId() == -1) &&
                                 Input::IsKeyDown(Key::Space) &&
                                 Input::IsMouseButtonPressed(MouseButton::Left);

        if (mouseInViewport && !mouseOverCameraHud &&
            (Input::IsMouseButtonPressed(MouseButton::Middle) ||
             Input::IsMouseButtonPressed(MouseButton::Right) ||
             spacePanInitiated)) {
            m_isPanningCamera = true;
            m_panLastMouseX = mx;
            m_panLastMouseY = my;
            gui.UnfocusTextField();
        }

        if (m_isPanningCamera) {
            bool panHeld = Input::IsMouseButtonDown(MouseButton::Middle) ||
                           Input::IsMouseButtonDown(MouseButton::Right) ||
                           (Input::IsKeyDown(Key::Space) && Input::IsMouseButtonDown(MouseButton::Left));
            if (panHeld) {
                float dx = mx - m_panLastMouseX;
                float dy = my - m_panLastMouseY;
                m_camera.targetX -= dx / m_camera.zoom;
                m_camera.targetY -= dy / m_camera.zoom;
                m_panLastMouseX = mx;
                m_panLastMouseY = my;
                return;
            } else {
                m_isPanningCamera = false;
            }
        }

        // Convert mouse screen coordinates to 2D world coordinates for entity picking & dragging
        float worldMx = 0.0f;
        float worldMy = 0.0f;
        m_camera.ScreenToWorld(mx, my, worldMx, worldMy);

        // =====================================================================
        // 3. World-Space Entity Picking & Dragging (Left-Click in Viewport)
        // =====================================================================
        if (Input::IsMouseButtonPressed(MouseButton::Left) &&
            mouseInViewport && !mouseOverCameraHud && !Input::IsKeyDown(Key::Space)) {

            gui.UnfocusTextField();

            auto& entities = scene.GetEntities();
            std::vector<size_t> pickOrder;
            pickOrder.reserve(entities.size());
            for (size_t i = 0; i < entities.size(); ++i) {
                if (entities[i].active) {
                    pickOrder.push_back(i);
                }
            }

            // Sort in reverse render order (front-to-back) so clicking overlapping entities
            // selects the visually top-most entity.
            std::sort(pickOrder.begin(), pickOrder.end(),
                [&entities](size_t idxA, size_t idxB) {
                    const Entity& a = entities[idxA];
                    const Entity& b = entities[idxB];
                    if (a.sprite.sortingLayer != b.sprite.sortingLayer) {
                        return a.sprite.sortingLayer > b.sprite.sortingLayer;
                    }
                    if (a.sprite.zOrder != b.sprite.zOrder) {
                        return a.sprite.zOrder > b.sprite.zOrder;
                    }
                    return idxA > idxB;
                });

            bool hitAny = false;
            for (size_t idx : pickOrder) {
                Entity& e = entities[idx];

                float hitX = e.transform.x;
                float hitY = e.transform.y;
                float hitW = e.transform.width;
                float hitH = e.transform.height;

                if (!e.sprite.enabled && e.text.enabled) {
                    hitX = e.transform.x + e.text.offsetX;
                    hitY = e.transform.y + e.text.offsetY;
                    hitW = static_cast<float>(Renderer2D::MeasureTextWidth(e.text.content.c_str(), e.text.fontSize));
                    hitH = static_cast<float>(e.text.fontSize);
                }

                if (worldMx >= hitX && worldMx <= hitX + hitW &&
                    worldMy >= hitY && worldMy <= hitY + hitH) {
                    undoRedo.BeginContinuousEdit(
                        scene,
                        inOutSelectedEntityId,
                        gui.GetTextCursorPos(),
                        gui.GetTextSelectionAnchor(),
                        isPlaying
                    );
                    inOutSelectedEntityId = e.id;
                    m_isDraggingEntity = true;
                    m_dragOffsetX = worldMx - e.transform.x;
                    m_dragOffsetY = worldMy - e.transform.y;
                    hitAny = true;
                    break;
                }
            }

            if (!hitAny) {
                inOutSelectedEntityId = 0;
            }
        }

        if (m_isDraggingEntity && Input::IsMouseButtonDown(MouseButton::Left)) {
            Entity* selected = scene.FindEntityById(inOutSelectedEntityId);
            if (selected) {
                selected->transform.x = worldMx - m_dragOffsetX;
                selected->transform.y = worldMy - m_dragOffsetY;
            }
        }
    }

    void CanvasViewport::RenderWorld(Scene::Scene& scene,
                                     int windowWidth,
                                     int windowHeight,
                                     float consoleHeight,
                                     Scene::EntityID selectedEntityId,
                                     bool isPlaying) const {
        int vpX = static_cast<int>(LEFT_PANEL_WIDTH);
        int vpY = static_cast<int>(TOOLBAR_HEIGHT);
        int vpW = windowWidth - static_cast<int>(LEFT_PANEL_WIDTH + RIGHT_PANEL_WIDTH);
        int vpH = windowHeight - static_cast<int>(TOOLBAR_HEIGHT + consoleHeight);

        Camera2DView activeCamera = m_camera;
        bool usingRuntimeCamera = false;
        if (isPlaying) {
            usingRuntimeCamera = scene.ResolveRuntimeCamera(
                static_cast<float>(vpX),
                static_cast<float>(vpY),
                static_cast<float>(vpW),
                static_cast<float>(vpH),
                activeCamera
            );
        }

        Renderer2D::BeginViewportScissor(vpX, vpY, vpW, vpH);
        Renderer2D::BeginCamera2D(activeCamera);

        if (!isPlaying) {
            RenderGrid(windowWidth, windowHeight, consoleHeight);
        }

        scene.Render();

        if (!isPlaying) {
            RenderSelectionGizmo(scene, selectedEntityId, vpW, vpH);
        }

        Renderer2D::EndCamera2D();

        // In Play Mode, display a compact Runtime Camera status badge in the bottom-left of the viewport
        if (isPlaying) {
            const Entity* camEntity = scene.FindActiveCameraEntity();
            float badgeX = static_cast<float>(vpX) + 12.0f;
            float badgeY = static_cast<float>(vpY + vpH) - 34.0f;
            char camInfo[112];
            if (camEntity && usingRuntimeCamera) {
                const char* modeStr = !camEntity->camera.followTarget
                    ? "Fixed"
                    : (camEntity->camera.smoothFollow ? "Smooth Follow" : "Instant Follow");
                const char* targetStr = (!camEntity->camera.followTarget)
                    ? camEntity->name.c_str()
                    : ((camEntity->camera.targetName.empty() || camEntity->camera.targetName == "Self")
                        ? camEntity->name.c_str()
                        : camEntity->camera.targetName.c_str());
                std::snprintf(camInfo, sizeof(camInfo),
                              "RUNTIME CAMERA: %s -> %s [%s | %.0f%%]",
                              camEntity->name.c_str(), targetStr, modeStr,
                              camEntity->camera.zoom * 100.0f);
            } else {
                std::snprintf(camInfo, sizeof(camInfo), "RUNTIME CAMERA: None (Using Editor View)");
            }
            int textW = Renderer2D::MeasureTextWidth(camInfo, 14);
            float pillW = static_cast<float>(textW) + 20.0f;
            Renderer2D::DrawRect(badgeX, badgeY, pillW, 24.0f, { 16, 20, 28, 215 });
            Renderer2D::DrawRectOutline(badgeX, badgeY, pillW, 24.0f, 1.0f, { 80, 195, 215, 200 });
            Renderer2D::DrawLabel(camInfo, badgeX + 10.0f, badgeY + 5.0f, 14, { 120, 235, 225, 255 });
        }

        Renderer2D::EndViewportScissor();
    }

    void CanvasViewport::RenderGrid(int windowWidth, int windowHeight, float consoleHeight) const {
        float vpLeft   = LEFT_PANEL_WIDTH;
        float vpRight  = static_cast<float>(windowWidth) - RIGHT_PANEL_WIDTH;
        float vpTop    = TOOLBAR_HEIGHT;
        float vpBottom = static_cast<float>(windowHeight) - consoleHeight;

        float worldLeft = 0.0f, worldTop = 0.0f;
        float worldRight = 0.0f, worldBottom = 0.0f;
        m_camera.ScreenToWorld(vpLeft, vpTop, worldLeft, worldTop);
        m_camera.ScreenToWorld(vpRight, vpBottom, worldRight, worldBottom);

        const float gridStep = 40.0f;
        float startX = std::floor(worldLeft / gridStep) * gridStep;
        float startY = std::floor(worldTop / gridStep) * gridStep;

        ColorRGBA gridColor      = { 38, 39, 46, 255 };
        ColorRGBA majorAxisColor = { 68, 74, 92, 255 };

        for (float x = startX; x <= worldRight; x += gridStep) {
            bool isOriginAxis = (std::fabs(x) < 0.5f);
            Renderer2D::DrawLine2D(x, worldTop, x, worldBottom, isOriginAxis ? majorAxisColor : gridColor);
        }
        for (float y = startY; y <= worldBottom; y += gridStep) {
            bool isOriginAxis = (std::fabs(y) < 0.5f);
            Renderer2D::DrawLine2D(worldLeft, y, worldRight, y, isOriginAxis ? majorAxisColor : gridColor);
        }
    }

    void CanvasViewport::RenderSelectionGizmo(const Scene::Scene& scene,
                                              Scene::EntityID selectedEntityId,
                                              int vpW,
                                              int vpH) const {
        float invZoom = (m_camera.zoom > 0.01f) ? (1.0f / m_camera.zoom) : 1.0f;
        const Entity* activeCamEntity = scene.FindActiveCameraEntity();

        for (const auto& e : scene.GetEntities()) {
            if (!e.active) continue;
            if (e.collider.enabled) {
                ColorRGBA colColor = e.collider.isStatic
                    ? ColorRGBA{ 75, 220, 120, 155 }
                    : ColorRGBA{ 90, 240, 195, 175 };
                float lineThick = std::max(1.0f, 1.4f * invZoom);
                if (e.collider.shape == Physics::ColliderShape::Circle) {
                    float cx = e.transform.x + e.transform.width * 0.5f;
                    float cy = e.transform.y + e.transform.height * 0.5f;
                    float r  = e.collider.GetEffectiveRadius(e.transform.width, e.transform.height);
                    Renderer2D::DrawCircleOutline(cx, cy, r, lineThick, colColor);
                    Renderer2D::DrawLine2D(cx, cy, cx + r, cy, colColor);
                } else {
                    Renderer2D::DrawRectOutline(
                        e.transform.x, e.transform.y,
                        e.transform.width, e.transform.height,
                        lineThick, colColor
                    );
                }
            }

            // Render Runtime Camera 2D Frustum Gizmo in Edit Mode
            if (e.camera.enabled && vpW > 0 && vpH > 0) {
                const Entity* target = &e;
                if (e.camera.followTarget && !e.camera.targetName.empty() && e.camera.targetName != "Self") {
                    const Entity* found = scene.FindEntity(e.camera.targetName);
                    if (found && found->active) {
                        target = found;
                    }
                }

                float camCenterX = target->transform.x + target->transform.width * 0.5f + e.camera.offsetX;
                float camCenterY = target->transform.y + target->transform.height * 0.5f + e.camera.offsetY;
                float camZoom = std::max(0.20f, std::min(5.00f, e.camera.zoom));

                float frustumW = static_cast<float>(vpW) / camZoom;
                float frustumH = static_cast<float>(vpH) / camZoom;
                float frustumX = camCenterX - frustumW * 0.5f;
                float frustumY = camCenterY - frustumH * 0.5f;

                bool isActivePrimary = (activeCamEntity && activeCamEntity->id == e.id);
                ColorRGBA camColor = isActivePrimary
                    ? ColorRGBA{ 90, 225, 245, 210 }
                    : ColorRGBA{ 130, 165, 195, 145 };

                Renderer2D::DrawRectOutline(
                    frustumX, frustumY, frustumW, frustumH,
                    std::max(1.5f, 1.8f * invZoom), camColor
                );

                // Center lens crosshair at (camCenterX, camCenterY)
                float crossArm = 10.0f * invZoom;
                Renderer2D::DrawLine2D(camCenterX - crossArm, camCenterY, camCenterX + crossArm, camCenterY, camColor);
                Renderer2D::DrawLine2D(camCenterX, camCenterY - crossArm, camCenterX, camCenterY + crossArm, camColor);

                // Guide line from owning entity center to camera framing center when offset or external target is used
                float ownerCenterX = e.transform.x + e.transform.width * 0.5f;
                float ownerCenterY = e.transform.y + e.transform.height * 0.5f;
                if (std::fabs(camCenterX - ownerCenterX) > 1.0f || std::fabs(camCenterY - ownerCenterY) > 1.0f) {
                    Renderer2D::DrawLine2D(ownerCenterX, ownerCenterY, camCenterX, camCenterY, { 90, 225, 245, 120 });
                }

                char camTag[64];
                std::snprintf(camTag, sizeof(camTag), "CAM: %s (%s %.0f%%)",
                              e.name.c_str(),
                              e.camera.followTarget ? "Follow" : "Fixed",
                              camZoom * 100.0f);
                Renderer2D::DrawLabel(camTag, frustumX + 6.0f * invZoom, frustumY + 5.0f * invZoom, 14, camColor);
            }
        }

        const Entity* selected = scene.FindEntityById(selectedEntityId);
        if (!selected || !selected->active) return;

        float bx = selected->transform.x;
        float by = selected->transform.y;
        float bw = selected->transform.width;
        float bh = selected->transform.height;

        if (!selected->sprite.enabled && selected->text.enabled) {
            bx = selected->transform.x + selected->text.offsetX;
            by = selected->transform.y + selected->text.offsetY;
            bw = static_cast<float>(Renderer2D::MeasureTextWidth(selected->text.content.c_str(), selected->text.fontSize));
            bh = static_cast<float>(selected->text.fontSize);
        }

        float pad = 3.0f * invZoom;
        Renderer2D::DrawRectOutline(
            bx - pad, by - pad, bw + pad * 2.0f, bh + pad * 2.0f,
            std::max(1.5f, 2.0f * invZoom), { 255, 215, 60, 255 }
        );
    }

    void CanvasViewport::RenderCameraOverlay(const Scene::Scene& scene,
                                             int windowWidth,
                                             int windowHeight,
                                             float consoleHeight,
                                             Scene::EntityID selectedEntityId,
                                             EditorGui& gui,
                                             std::string& outStatusMessage) {
        float hudX = LEFT_PANEL_WIDTH + 12.0f;
        float hudY = static_cast<float>(windowHeight) - consoleHeight - 38.0f;

        Renderer2D::DrawRect(hudX, hudY, 232.0f, 28.0f, { 20, 22, 28, 220 });
        Renderer2D::DrawRectOutline(hudX, hudY, 232.0f, 28.0f, 1.0f, { 65, 70, 88, 220 });

        char zoomBuf[32];
        std::snprintf(zoomBuf, sizeof(zoomBuf), "Zoom: %.0f%%", m_camera.zoom * 100.0f);
        Renderer2D::DrawLabel(zoomBuf, hudX + 10.0f, hudY + 6.0f, 14, { 210, 220, 240, 255 });

        if (gui.DrawButton(hudX + 98.0f, hudY + 3.0f, 60.0f, 22.0f, "Reset",
                           { 45, 50, 65, 255 }, { 65, 72, 94, 255 })) {
            outStatusMessage = ResetCamera();
        }
        if (gui.DrawButton(hudX + 164.0f, hudY + 3.0f, 62.0f, 22.0f, "Focus [F]",
                           { 45, 85, 135, 255 }, { 60, 110, 175, 255 })) {
            outStatusMessage = FocusSelectedEntity(scene, selectedEntityId, windowWidth, windowHeight, consoleHeight);
        }

        Renderer2D::DrawLabel(
            "Pan: Right/Middle Drag or Space+Drag   |   Zoom: Mouse Wheel",
            hudX + 244.0f, hudY + 7.0f, 14, { 125, 130, 148, 220 }
        );
    }

}
