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
            m_addComponentMenuOpen = false;
            m_lastSelectedEntityId = 0;
            Renderer2D::DrawLabel("Select any entity in the viewport", panelX + 20.0f, TOOLBAR_HEIGHT + 65.0f, 16, { 120, 125, 140, 255 });
            Renderer2D::DrawLabel("or from the left panel to edit its", panelX + 20.0f, TOOLBAR_HEIGHT + 88.0f, 16, { 120, 125, 140, 255 });
            Renderer2D::DrawLabel("properties directly.", panelX + 20.0f, TOOLBAR_HEIGHT + 111.0f, 16, { 120, 125, 140, 255 });
            return;
        }

        if (selectedEntityId != m_lastSelectedEntityId) {
            m_addComponentMenuOpen = false;
            m_lastSelectedEntityId = selectedEntityId;
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

        // Helper lambda to draw a component section header bar with a right-aligned [X] remove button
        auto drawComponentHeader = [&](const char* title, const char* badgeText, ColorRGBA badgeColor, bool& enabledFlag) {
            Renderer2D::DrawLine2D(x, y - 4.0f, x + w, y - 4.0f, { 42, 45, 56, 255 });
            Renderer2D::DrawRect(x, y, w, 22.0f, { 32, 35, 45, 255 });
            Renderer2D::DrawLabel(title, x + 8.0f, y + 3.0f, 15, { 95, 195, 255, 255 });

            float removeBtnW = 22.0f;
            float removeBtnX = x + w - removeBtnW - 2.0f;
            if (badgeText && badgeText[0] != '\0') {
                int bw = Renderer2D::MeasureTextWidth(badgeText, 13);
                Renderer2D::DrawLabel(badgeText, removeBtnX - static_cast<float>(bw) - 8.0f, y + 4.0f, 13, badgeColor);
            }

            if (gui.DrawButton(removeBtnX, y + 2.0f, removeBtnW, 18.0f, "X",
                               { 130, 48, 55, 255 }, { 175, 62, 70, 255 })) {
                gui.PushUndoIfActive();
                enabledFlag = false;
            }
            y += 27.0f;
        };

        // 1. Entity Identity, Tag & Master Active Toggle
        gui.DrawCheckbox(x, y, "Active", selected->active);
        if (!selected->tag.empty()) {
            std::string tagBadge = "[" + selected->tag + "]";
            int tbW = Renderer2D::MeasureTextWidth(tagBadge.c_str(), 14);
            Renderer2D::DrawLabel(tagBadge.c_str(), x + w - static_cast<float>(tbW), y + 2.0f, 14, { 110, 225, 165, 255 });
        } else {
            Renderer2D::DrawLabel("[Untagged]", x + w - 76.0f, y + 2.0f, 14, { 120, 125, 140, 255 });
        }
        y += 22.0f;
        gui.DrawTextInputBox(x, y, w, "Name:", selected->name, 1);
        y += 27.0f;
        gui.DrawTextInputBox(x, y, w, "Tag:", selected->tag, 6);
        y += 26.0f;

        // Quick preset tag buttons ("Player", "Enemy", "Coin", "Wall", "None")
        static const char* TAG_PRESETS[5] = { "Player", "Enemy", "Coin", "Wall", "None" };
        float tagBtnGap = 4.0f;
        float tagBtnW = (w - tagBtnGap * 4.0f) / 5.0f;
        for (int t = 0; t < 5; ++t) {
            const char* preset = TAG_PRESETS[t];
            bool isClear = (t == 4);
            bool isActiveTag = isClear ? selected->tag.empty() : (selected->tag == preset);
            ColorRGBA bg = isActiveTag ? ColorRGBA{ 50, 125, 95, 255 } : ColorRGBA{ 38, 41, 52, 255 };
            ColorRGBA hv = isActiveTag ? ColorRGBA{ 65, 150, 115, 255 } : ColorRGBA{ 52, 56, 70, 255 };
            if (gui.DrawButton(x + static_cast<float>(t) * (tagBtnW + tagBtnGap), y, tagBtnW, 20.0f, preset, bg, hv)) {
                std::string nextTag = isClear ? "" : preset;
                if (selected->tag != nextTag) {
                    gui.PushUndoIfActive();
                    selected->tag = nextTag;
                }
                if (gui.GetFocusedTextFieldId() == 6) {
                    gui.ResetTextCaret();
                }
            }
        }
        y += 28.0f;

        // 2. Transform Component (Fundamental component: always present on every entity)
        Renderer2D::DrawLine2D(x, y - 4.0f, x + w, y - 4.0f, { 42, 45, 56, 255 });
        Renderer2D::DrawRect(x, y, w, 22.0f, { 32, 35, 45, 255 });
        Renderer2D::DrawLabel("TRANSFORM 2D", x + 8.0f, y + 3.0f, 15, { 95, 195, 255, 255 });
        Renderer2D::DrawLabel("[Fundamental]", x + w - 96.0f, y + 4.0f, 13, { 130, 145, 170, 255 });
        y += 26.0f;
        gui.DrawFloatSlider(x, y, w, "Pos X", selected->transform.x, 230.0f, 980.0f, 10);
        y += 21.0f;
        gui.DrawFloatSlider(x, y, w, "Pos Y", selected->transform.y, 50.0f, 740.0f, 11);
        y += 21.0f;
        gui.DrawFloatSlider(x, y, w, "Width", selected->transform.width, 16.0f, 500.0f, 12);
        y += 21.0f;
        gui.DrawFloatSlider(x, y, w, "Height", selected->transform.height, 16.0f, 500.0f, 13);
        y += 26.0f;

        // 3. Render Depth (Sorting Layer & Z-Order) — shown when a visual component (Sprite or Text) is attached
        if (selected->sprite.enabled || selected->text.enabled) {
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
        }

        // 4. Sprite 2D Component (only rendered when added to the entity)
        if (selected->sprite.enabled) {
            const char* spriteBadge = "[Solid Box]";
            ColorRGBA spriteBadgeColor = { 135, 140, 155, 255 };
            if (selected->sprite.texturePath.empty()) {
                if (selected->collider.enabled && selected->collider.shape == Physics::ColliderShape::Circle) {
                    spriteBadge = "[Solid Circle]";
                }
            } else if (Renderer2D::HasTexture(selected->sprite.texturePath.c_str())) {
                spriteBadge = "[Texture OK]";
                spriteBadgeColor = { 90, 225, 135, 255 };
            } else {
                spriteBadge = "[Missing!]";
                spriteBadgeColor = { 245, 95, 95, 255 };
            }

            drawComponentHeader("SPRITE 2D", spriteBadge, spriteBadgeColor, selected->sprite.enabled);
            if (selected->sprite.enabled) {
                gui.DrawTextureSelector(x, y, w, selected->sprite);
                y += 55.0f;

                gui.DrawSpriteRegionControls(x, y, w, selected->sprite, 60);
                y += 128.0f;

                gui.DrawSpriteAnimatorControls(x, y, w, selected->sprite, 70);
                y += 110.0f;

                const char* colorLabel = selected->sprite.texturePath.empty() ? "Shape Color" : "Img Tint";
                gui.DrawColorPickerRow(x, y, w, colorLabel, selected->sprite.color, 20);
                y += 84.0f;
            }
        }

        // 5. Text Label Component (only rendered when added to the entity)
        if (selected->text.enabled) {
            drawComponentHeader("TEXT LABEL", "[2D Text]", { 215, 220, 235, 255 }, selected->text.enabled);
            if (selected->text.enabled) {
                gui.DrawTextInputBox(x, y, w, "Text:", selected->text.content, 2);
                y += 27.0f;
                gui.DrawIntSlider(x, y, w, "Font Size", selected->text.fontSize, 12, 56, 30);
                y += 21.0f;
                gui.DrawFloatSlider(x, y, w, "Offset Y", selected->text.offsetY, -80.0f, 80.0f, 31);
                y += 23.0f;
                gui.DrawColorPickerRow(x, y, w, "Font Color", selected->text.color, 40);
                y += 84.0f;
            }
        }

        // 6. Collider 2D Component (only rendered when added to the entity)
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
            ColorRGBA badgeColor = selected->collider.isTrigger
                ? ColorRGBA{ 255, 195, 70, 255 }
                : ColorRGBA{ 90, 225, 135, 255 };

            drawComponentHeader("COLLIDER 2D", colBadge, badgeColor, selected->collider.enabled);
            if (selected->collider.enabled) {
                y += gui.DrawColliderControls(x, y, w, selected->collider, selected->transform, 90);
                y += 4.0f;
            }
        }

        // 7. Movement 2D Component (only rendered when added to the entity)
        if (selected->movement.enabled) {
            const char* moveBadge = selected->movement.playerControlled ? "[WASD Player]" : "[Physics Body]";
            drawComponentHeader("MOVEMENT 2D", moveBadge, { 130, 210, 255, 255 }, selected->movement.enabled);
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
                y += 26.0f;
            }
        }

        // 8. Camera 2D Component (only rendered when added to the entity)
        if (selected->camera.enabled) {
            char camBadge[40];
            std::snprintf(camBadge, sizeof(camBadge), "[%s | %.0f%%]",
                          selected->camera.followTarget ? "Follow" : "Fixed",
                          selected->camera.zoom * 100.0f);
            drawComponentHeader("CAMERA 2D", camBadge, { 110, 225, 215, 255 }, selected->camera.enabled);
            if (selected->camera.enabled) {
                y += gui.DrawCameraControls(x, y, w, selected->camera, 80);
                y += 4.0f;
            }
        }

        // 9. Bottom "Add Component" Button & Dropdown List
        Renderer2D::DrawLine2D(x, y, x + w, y, { 50, 54, 68, 255 });
        y += 10.0f;

        const char* addBtnLabel = m_addComponentMenuOpen ? "v  Add Component" : "+  Add Component";
        ColorRGBA addBtnBg = m_addComponentMenuOpen ? ColorRGBA{ 48, 105, 185, 255 } : ColorRGBA{ 45, 88, 155, 255 };
        ColorRGBA addBtnHv = m_addComponentMenuOpen ? ColorRGBA{ 62, 125, 215, 255 } : ColorRGBA{ 60, 112, 192, 255 };
        if (gui.DrawButton(x, y, w, 28.0f, addBtnLabel, addBtnBg, addBtnHv)) {
            m_addComponentMenuOpen = !m_addComponentMenuOpen;
        }
        y += 32.0f;

        if (m_addComponentMenuOpen) {
            struct ComponentOption {
                const char* label;
                bool* enabledPtr;
                bool isMovement;
            };
            ComponentOption options[5] = {
                { "+  Sprite 2D (Image / Shape)",     &selected->sprite.enabled,   false },
                { "+  Text Label (2D Typography)",    &selected->text.enabled,     false },
                { "+  Collider 2D (Box / Circle)",    &selected->collider.enabled, false },
                { "+  Movement 2D (WASD / Physics)",  &selected->movement.enabled, true  },
                { "+  Camera 2D (Runtime View)",      &selected->camera.enabled,   false }
            };

            int availableCount = 0;
            for (const auto& opt : options) {
                if (!(*opt.enabledPtr)) {
                    ++availableCount;
                }
            }

            float menuBoxH = (availableCount > 0) ? (static_cast<float>(availableCount) * 28.0f + 8.0f) : 34.0f;
            Renderer2D::DrawRect(x, y, w, menuBoxH, { 18, 20, 26, 255 });
            Renderer2D::DrawRectOutline(x, y, w, menuBoxH, 1.0f, { 75, 130, 205, 255 });
            y += 4.0f;

            if (availableCount == 0) {
                Renderer2D::DrawLabel("All components are already attached.", x + 12.0f, y + 6.0f, 14, { 140, 148, 165, 255 });
                y += 30.0f;
            } else {
                for (const auto& opt : options) {
                    if (*opt.enabledPtr) {
                        continue;
                    }
                    if (gui.DrawButton(x + 4.0f, y, w - 8.0f, 24.0f, opt.label,
                                       { 32, 36, 48, 255 }, { 52, 95, 160, 255 })) {
                        gui.PushUndoIfActive();
                        *opt.enabledPtr = true;
                        if (opt.isMovement) {
                            selected->movement.playerControlled = true;
                        }
                        m_addComponentMenuOpen = false;
                    }
                    y += 28.0f;
                }
                y += 4.0f;
            }
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
