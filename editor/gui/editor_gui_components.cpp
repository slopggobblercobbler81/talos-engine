#include "editor/gui/editor_gui.h"
#include "core/input.h"
#include <algorithm>
#include <cmath>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Scene;

    float EditorGui::DrawColliderControls(float x, float y, float w,
                                          Scene::ColliderComponent& collider,
                                          const Scene::TransformComponent& transform,
                                          int baseSliderId) {
        float cy = y;

        // 1. Shape Toggle Row: [Box (AABB)] / [Circle]
        Renderer2D::DrawLabel("Shape:", x, cy + 3.0f, 14, { 185, 190, 205, 255 });

        bool isBox    = (collider.shape == Physics::ColliderShape::Box);
        bool isCircle = (collider.shape == Physics::ColliderShape::Circle);

        float btnStartX = x + 54.0f;
        float availW    = w - 54.0f;
        float gap       = 6.0f;
        float btnW      = (availW - gap) * 0.5f;

        if (DrawButton(btnStartX, cy, btnW, 20.0f, "Box (AABB)",
                       isBox ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 },
                       { 68, 135, 220, 255 })) {
            if (!isBox) {
                PushUndoIfActive();
            }
            collider.shape = Physics::ColliderShape::Box;
            m_focusedTextFieldId = -1;
        }

        if (DrawButton(btnStartX + btnW + gap, cy, btnW, 20.0f, "Circle",
                       isCircle ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 },
                       { 68, 135, 220, 255 })) {
            if (!isCircle) {
                PushUndoIfActive();
            }
            collider.shape = Physics::ColliderShape::Circle;
            m_focusedTextFieldId = -1;
        }
        cy += 24.0f;

        // 2. Solid & Static Body Flags
        DrawCheckbox(x, cy, "Solid Collision", collider.isSolid);
        DrawCheckbox(x + w * 0.52f, cy, "Static Body", collider.isStatic);
        cy += 23.0f;

        // 3. Circle Radius Slider + [Auto] Fit Button (shown when Shape == Circle)
        if (collider.shape == Physics::ColliderShape::Circle) {
            bool isAutoRadius = (collider.radius <= 0.5f);
            float displayRadius = collider.GetEffectiveRadius(transform.width, transform.height);
            float prevRadius = displayRadius;

            DrawFloatSlider(x, cy, w - 44.0f, "Radius", displayRadius, 4.0f, 250.0f, baseSliderId + 0);
            if (std::fabs(displayRadius - prevRadius) > 0.01f) {
                collider.radius = std::max(4.0f, std::round(displayRadius));
            }

            if (DrawButton(x + w - 40.0f, cy, 40.0f, 17.0f, "Auto",
                           isAutoRadius ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                           { 64, 128, 210, 255 })) {
                if (!isAutoRadius) {
                    PushUndoIfActive();
                }
                collider.radius = 0.0f;
                m_focusedTextFieldId = -1;
            }
            cy += 21.0f;
        }

        // 4. Bounciness / Restitution Slider (0% = Inelastic, 100% = Elastic Bounce)
        float bouncePct = std::round(collider.restitution * 100.0f);
        DrawFloatSlider(x, cy, w, "Bounce (%)", bouncePct, 0.0f, 100.0f, baseSliderId + 1);
        collider.restitution = std::max(0.0f, std::min(1.0f, std::round(bouncePct) / 100.0f));
        cy += 23.0f;

        return cy - y;
    }

    float EditorGui::DrawCameraControls(float x, float y, float w, Scene::CameraComponent& camera, int baseSliderId) {
        float cy = y;

        // 1. Camera Mode Row: [Fixed] / [Follow] toggle buttons + Primary checkbox
        Renderer2D::DrawLabel("Mode:", x, cy + 3.0f, 14, { 185, 190, 205, 255 });

        bool isFixed  = !camera.followTarget;
        bool isFollow = camera.followTarget;

        if (DrawButton(x + 48.0f, cy, 62.0f, 20.0f, "Fixed",
                       isFixed ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 },
                       { 68, 135, 220, 255 })) {
            if (!isFixed) {
                PushUndoIfActive();
            }
            camera.followTarget = false;
            camera.runtimeInitialized = false;
            m_focusedTextFieldId = -1;
        }

        if (DrawButton(x + 114.0f, cy, 64.0f, 20.0f, "Follow",
                       isFollow ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 },
                       { 68, 135, 220, 255 })) {
            if (!isFollow) {
                PushUndoIfActive();
            }
            camera.followTarget = true;
            camera.runtimeInitialized = false;
            m_focusedTextFieldId = -1;
        }

        DrawCheckbox(x + 190.0f, cy + 1.0f, "Primary", camera.isPrimary);
        cy += 25.0f;

        // 2. Follow Target Controls (Target entity name + [Self] / [Player] presets + Smooth Damping)
        if (camera.followTarget) {
            DrawTextInputBox(x, cy, w - 96.0f, "Target:", camera.targetName, 5);

            bool isSelfTarget = (camera.targetName.empty() || camera.targetName == "Self");
            if (DrawButton(x + w - 92.0f, cy, 40.0f, 23.0f, "Self",
                           isSelfTarget ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                           { 64, 128, 210, 255 })) {
                if (!isSelfTarget) {
                    PushUndoIfActive();
                }
                camera.targetName = "";
                camera.runtimeInitialized = false;
                m_focusedTextFieldId = -1;
            }

            bool isPlayerTarget = (camera.targetName == "Player");
            if (DrawButton(x + w - 48.0f, cy, 48.0f, 23.0f, "Player",
                           isPlayerTarget ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                           { 64, 128, 210, 255 })) {
                if (!isPlayerTarget) {
                    PushUndoIfActive();
                }
                camera.targetName = "Player";
                camera.runtimeInitialized = false;
                m_focusedTextFieldId = -1;
            }
            cy += 27.0f;

            DrawCheckbox(x, cy, "Smooth Follow (Damping)", camera.smoothFollow);
            cy += 22.0f;

            if (camera.smoothFollow) {
                DrawFloatSlider(x, cy, w, "Follow Spd", camera.followSpeed, 1.0f, 25.0f, baseSliderId + 0);
                camera.followSpeed = std::round(camera.followSpeed);
                cy += 21.0f;
            }
        }

        // 3. Zoom & Framing Offset Sliders
        float zoomPct = std::round(camera.zoom * 100.0f);
        DrawFloatSlider(x, cy, w - 42.0f, "Zoom (%)", zoomPct, 25.0f, 400.0f, baseSliderId + 1);
        camera.zoom = std::max(0.25f, std::min(4.00f, std::round(zoomPct) / 100.0f));

        bool isZoom1x = (std::fabs(camera.zoom - 1.0f) < 0.01f);
        if (DrawButton(x + w - 38.0f, cy, 38.0f, 17.0f, "100%",
                       isZoom1x ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                       { 64, 128, 210, 255 })) {
            if (!isZoom1x) {
                PushUndoIfActive();
            }
            camera.zoom = 1.0f;
            m_focusedTextFieldId = -1;
        }
        cy += 21.0f;

        float prevOffX = camera.offsetX;
        float prevOffY = camera.offsetY;
        DrawFloatSlider(x, cy,         w, "Offset X", camera.offsetX, -300.0f, 300.0f, baseSliderId + 2);
        DrawFloatSlider(x, cy + 21.0f, w, "Offset Y", camera.offsetY, -300.0f, 300.0f, baseSliderId + 3);
        camera.offsetX = std::round(camera.offsetX);
        camera.offsetY = std::round(camera.offsetY);
        if (std::fabs(camera.offsetX - prevOffX) > 0.01f || std::fabs(camera.offsetY - prevOffY) > 0.01f) {
            camera.runtimeInitialized = false;
        }
        cy += 44.0f;

        return cy - y;
    }

}
