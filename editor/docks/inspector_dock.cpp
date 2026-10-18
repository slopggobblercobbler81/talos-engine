#include "editor/docks/inspector_dock.h"
#include "core/input.h"
#include "renderer/renderer2d.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Scene;
    using namespace Talos::Editor::EditorLayout;

    void InspectorDock::Render(Scene::Scene& scene,
                               int windowWidth,
                               int windowHeight,
                               Scene::EntityID selectedEntityId,
                               EditorGui& gui) {
        float panelX = static_cast<float>(windowWidth) - RIGHT_PANEL_WIDTH;
        float panelH = static_cast<float>(windowHeight) - TOOLBAR_HEIGHT;

        // Right Panel Background
        Renderer2D::DrawRect(panelX, TOOLBAR_HEIGHT, RIGHT_PANEL_WIDTH, panelH, { 24, 25, 31, 255 });
        Renderer2D::DrawLine2D(panelX, TOOLBAR_HEIGHT, panelX, static_cast<float>(windowHeight), { 55, 58, 70, 255 });

        Renderer2D::DrawLabel("FIRST-LEVEL INSPECTOR", panelX + 16.0f, TOOLBAR_HEIGHT + 14.0f, 16, { 150, 155, 175, 255 });
        Renderer2D::DrawLine2D(panelX + 12.0f, TOOLBAR_HEIGHT + 36.0f, panelX + RIGHT_PANEL_WIDTH - 12.0f, TOOLBAR_HEIGHT + 36.0f, { 45, 48, 58, 255 });

        Entity* selected = scene.FindEntityById(selectedEntityId);
        if (!selected) {
            Renderer2D::DrawLabel("Select any entity in the viewport", panelX + 20.0f, TOOLBAR_HEIGHT + 65.0f, 16, { 120, 125, 140, 255 });
            Renderer2D::DrawLabel("or from the left panel to edit its", panelX + 20.0f, TOOLBAR_HEIGHT + 88.0f, 16, { 120, 125, 140, 255 });
            Renderer2D::DrawLabel("properties directly.", panelX + 20.0f, TOOLBAR_HEIGHT + 111.0f, 16, { 120, 125, 140, 255 });
            return;
        }

        float contentTopY = TOOLBAR_HEIGHT + 38.0f;
        float viewH = static_cast<float>(windowHeight) - contentTopY;
        float maxScroll = std::max(0.0f, m_contentHeight - viewH + 16.0f);

        if (EditorGui::IsMouseOver(panelX, contentTopY, RIGHT_PANEL_WIDTH, viewH)) {
            float wheel = Input::GetMouseWheelMove();
            if (std::fabs(wheel) > 0.001f) {
                m_scrollOffset -= wheel * 28.0f;
            }
        }
        m_scrollOffset = std::max(0.0f, std::min(m_scrollOffset, maxScroll));

        Renderer2D::BeginViewportScissor(
            static_cast<int>(panelX),
            static_cast<int>(contentTopY),
            static_cast<int>(RIGHT_PANEL_WIDTH),
            static_cast<int>(viewH)
        );

        float x = panelX + 16.0f;
        float w = RIGHT_PANEL_WIDTH - 32.0f;
        float startY = TOOLBAR_HEIGHT + 44.0f - m_scrollOffset;
        float y = startY;

        // 1. Entity Identity & Master Active Toggle
        gui.DrawCheckbox(x, y, "Active", selected->active);
        y += 22.0f;
        gui.DrawTextInputBox(x, y, w, "Name:", selected->name, 1);
        y += 29.0f;

        // 2. Transform Component (Always accessible on first level)
        Renderer2D::DrawLabel("TRANSFORM 2D", x, y, 15, { 95, 195, 255, 255 });
        y += 18.0f;
        gui.DrawFloatSlider(x, y, w, "Pos X", selected->transform.x, 230.0f, 980.0f, 10);
        y += 21.0f;
        gui.DrawFloatSlider(x, y, w, "Pos Y", selected->transform.y, 50.0f, 740.0f, 11);
        y += 21.0f;
        gui.DrawFloatSlider(x, y, w, "Width", selected->transform.width, 16.0f, 500.0f, 12);
        y += 21.0f;
        gui.DrawFloatSlider(x, y, w, "Height", selected->transform.height, 16.0f, 500.0f, 13);
        y += 25.0f;

        // 3. Render Depth (Sorting Layer & Z-Order)
        Renderer2D::DrawLabel("RENDER DEPTH", x, y, 15, { 250, 200, 75, 255 });
        char depthHeaderBuf[48];
        std::snprintf(depthHeaderBuf, sizeof(depthHeaderBuf), "[L%d: %s | Z: %d]",
                      selected->sprite.sortingLayer,
                      SortingLayer::GetName(selected->sprite.sortingLayer),
                      selected->sprite.zOrder);
        int dhWidth = Renderer2D::MeasureTextWidth(depthHeaderBuf, 14);
        Renderer2D::DrawLabel(depthHeaderBuf, x + w - static_cast<float>(dhWidth), y + 1.0f, 14, { 190, 210, 240, 255 });
        y += 18.0f;

        gui.DrawSortingControls(x, y, w, selected->sprite.sortingLayer, selected->sprite.zOrder, 15);
        y += 49.0f;

        // 4. Sprite Component (2D Texture / Solid Shape + Color / Tint)
        gui.DrawCheckbox(x, y, "SPRITE (2D Image / Shape)", selected->sprite.enabled);
        if (selected->sprite.enabled) {
            if (selected->sprite.texturePath.empty()) {
                if (selected->collider.enabled && selected->collider.shape == Physics::ColliderShape::Circle) {
                    Renderer2D::DrawLabel("[Solid Circle]", x + w - 92.0f, y + 2.0f, 14, { 135, 140, 155, 255 });
                } else {
                    Renderer2D::DrawLabel("[Solid Box]", x + w - 74.0f, y + 2.0f, 14, { 135, 140, 155, 255 });
                }
            } else if (Renderer2D::HasTexture(selected->sprite.texturePath.c_str())) {
                Renderer2D::DrawLabel("[Texture OK]", x + w - 82.0f, y + 2.0f, 14, { 90, 225, 135, 255 });
            } else {
                Renderer2D::DrawLabel("[Missing!]", x + w - 68.0f, y + 2.0f, 14, { 245, 95, 95, 255 });
            }
        }
        y += 23.0f;

        if (selected->sprite.enabled) {
            gui.DrawTextureSelector(x, y, w, selected->sprite);
            y += 55.0f;

            gui.DrawSpriteRegionControls(x, y, w, selected->sprite, 60);
            y += 128.0f;

            gui.DrawSpriteAnimatorControls(x, y, w, selected->sprite, 70);
            y += 110.0f;

            const char* colorLabel = selected->sprite.texturePath.empty() ? "Shape Color" : "Img Tint";
            gui.DrawColorPickerRow(x, y, w, colorLabel, selected->sprite.color, 20);
            y += 80.0f;
        }

        // 5. Text Component (First-Level Text, Font Size & Color)
        gui.DrawCheckbox(x, y, "TEXT LABEL (First-Level)", selected->text.enabled);
        y += 23.0f;
        if (selected->text.enabled) {
            gui.DrawTextInputBox(x, y, w, "Text:", selected->text.content, 2);
            y += 27.0f;
            gui.DrawIntSlider(x, y, w, "Font Size", selected->text.fontSize, 12, 56, 30);
            y += 21.0f;
            gui.DrawFloatSlider(x, y, w, "Offset Y", selected->text.offsetY, -80.0f, 80.0f, 31);
            y += 23.0f;
            gui.DrawColorPickerRow(x, y, w, "Font Color", selected->text.color, 40);
            y += 80.0f;
        }

        // 6. Collider 2D Component (Box AABB / Circle + Narrow-Phase Physics & Trigger Sensors)
        gui.DrawCheckbox(x, y, "COLLIDER 2D (Box / Circle)", selected->collider.enabled);
        if (selected->collider.enabled) {
            char colBadge[56];
            const char* modeLabel = selected->collider.isTrigger
                ? "Trigger"
                : (selected->collider.isStatic ? "Static" : "Dynamic");
            if (selected->collider.shape == Physics::ColliderShape::Circle) {
                float effR = selected->collider.GetEffectiveRadius(selected->transform.width, selected->transform.height);
                std::snprintf(colBadge, sizeof(colBadge), "[Circle R=%.0f | %s]", effR, modeLabel);
            } else {
                std::snprintf(colBadge, sizeof(colBadge), "[Box | %s]", modeLabel);
            }
            int colW = Renderer2D::MeasureTextWidth(colBadge, 14);
            ColorRGBA badgeColor = selected->collider.isTrigger
                ? ColorRGBA{ 255, 195, 70, 255 }
                : ColorRGBA{ 90, 225, 135, 255 };
            Renderer2D::DrawLabel(colBadge, x + w - static_cast<float>(colW), y + 2.0f, 14, badgeColor);
        }
        y += 23.0f;
        if (selected->collider.enabled) {
            y += gui.DrawColliderControls(x, y, w, selected->collider, selected->transform, 90);
        }

        // 7. Movement 2D Component (WASD Control & Rigid-Body Physics)
        bool wasMoveEnabled = selected->movement.enabled;
        if (gui.DrawCheckbox(x, y, "MOVEMENT (WASD / Physics)", selected->movement.enabled)) {
            if (!wasMoveEnabled && selected->movement.enabled) {
                selected->movement.playerControlled = true;
            }
        }
        y += 21.0f;
        if (selected->movement.enabled) {
            gui.DrawCheckbox(x + 14.0f, y, "Player Controlled (WASD)", selected->movement.playerControlled);
            y += 21.0f;
            if (selected->movement.playerControlled) {
                gui.DrawFloatSlider(x, y, w, "Speed", selected->movement.speed, 50.0f, 700.0f, 50);
                y += 21.0f;
            } else {
                gui.DrawFloatSlider(x, y, w, "Velocity X", selected->movement.velocityX, -500.0f, 500.0f, 51);
                y += 21.0f;
                gui.DrawFloatSlider(x, y, w, "Velocity Y", selected->movement.velocityY, -500.0f, 500.0f, 52);
                y += 21.0f;
            }
            gui.DrawFloatSlider(x, y, w, "Mass", selected->movement.mass, 1.0f, 20.0f, 53);
            y += 21.0f;
            float gravPct = std::round(selected->movement.gravityScale * 100.0f);
            gui.DrawFloatSlider(x, y, w, "Gravity (%)", gravPct, -200.0f, 200.0f, 54);
            selected->movement.gravityScale = std::round(gravPct) / 100.0f;
            y += 22.0f;
        }

        // 8. Runtime Game Camera Component (Play Mode & Exported Game View)
        gui.DrawCheckbox(x, y, "CAMERA 2D (Runtime View)", selected->camera.enabled);
        if (selected->camera.enabled) {
            char camBadge[40];
            std::snprintf(camBadge, sizeof(camBadge), "[%s | %.0f%%]",
                          selected->camera.followTarget ? "Follow" : "Fixed",
                          selected->camera.zoom * 100.0f);
            int cbWidth = Renderer2D::MeasureTextWidth(camBadge, 14);
            Renderer2D::DrawLabel(camBadge, x + w - static_cast<float>(cbWidth), y + 2.0f, 14, { 110, 225, 215, 255 });
        }
        y += 23.0f;
        if (selected->camera.enabled) {
            y += gui.DrawCameraControls(x, y, w, selected->camera, 80);
        }

        m_contentHeight = y - startY;

        // Draw subtle scrollbar thumb when content exceeds panel height
        if (maxScroll > 0.5f && viewH > 40.0f) {
            float trackX = panelX + RIGHT_PANEL_WIDTH - 5.0f;
            float thumbRatio = std::max(0.15f, std::min(1.0f, viewH / m_contentHeight));
            float thumbH = viewH * thumbRatio;
            float scrollT = (maxScroll > 0.0f) ? (m_scrollOffset / maxScroll) : 0.0f;
            float thumbY = contentTopY + scrollT * (viewH - thumbH);
            Renderer2D::DrawRect(trackX, contentTopY, 3.0f, viewH, { 34, 36, 45, 200 });
            Renderer2D::DrawRect(trackX, thumbY, 3.0f, thumbH, { 95, 145, 215, 220 });
        }

        Renderer2D::EndViewportScissor();
    }

}
