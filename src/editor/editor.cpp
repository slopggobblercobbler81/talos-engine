#include "editor/editor.h"
#include "scene/scene_serializer.h"
#include "core/input.h"
#include <cstdio>
#include <algorithm>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Scene;

    // Preset color swatches for instant 1-click color/tint selection in the Inspector.
    // Swatch #6 is pure White (255,255,255) so users can reset a sprite texture's tint with 1 click.
    static const ColorRGBA PRESET_COLORS[] = {
        { 80,  180, 250, 255 }, // Sky Blue
        { 235, 95,  95,  255 }, // Coral Red
        { 90,  215, 130, 255 }, // Emerald Green
        { 250, 200, 75,  255 }, // Warm Gold
        { 185, 115, 250, 255 }, // Vivid Purple
        { 250, 140, 70,  255 }, // Tangerine Orange
        { 255, 255, 255, 255 }, // Pure White (Unmodified Texture Tint)
        { 60,  65,  80,  255 }  // Slate Gray
    };
    static constexpr int NUM_PRESET_COLORS = 8;

    // Built-in 1-click texture presets pointing to real .png files in "assets/textures/"
    struct TexturePreset {
        const char* label;
        const char* path;
    };

    static const TexturePreset TEXTURE_PRESETS[] = {
        { "None",  "" },
        { "Hero",  "assets/textures/hero.png" },
        { "Crate", "assets/textures/crate.png" },
        { "Brick", "assets/textures/brick.png" },
        { "Coin",  "assets/textures/coin.png" }
    };
    static constexpr int NUM_TEXTURE_PRESETS = 5;

    // Helper to test if the mouse cursor lies inside a 2D UI rectangle
    static bool IsMouseOver(float x, float y, float w, float h) {
        float mx = Input::GetMouseX();
        float my = Input::GetMouseY();
        return (mx >= x && mx <= x + w && my >= y && my <= y + h);
    }

    Editor::Editor(int windowWidth, int windowHeight, const std::string& sceneFilePath)
        : m_windowWidth(windowWidth), m_windowHeight(windowHeight), m_sceneFilePath(sceneFilePath) {}

    void Editor::StartPlayMode(const Scene::Scene& scene) {
        if (m_isPlaying) return;

        // Capture a complete copy of all entities in memory before starting simulation.
        // Because Entities are flat POD-like structs, copying the vector takes microseconds.
        m_sceneSnapshot = scene.GetEntities();
        m_isPlaying = true;
        m_isDraggingEntity = false;
        m_focusedTextFieldId = -1;
    }

    void Editor::StopPlayMode(Scene::Scene& scene) {
        if (!m_isPlaying) return;

        // Instantly restore the pre-Play entity state (zero reload time).
        scene.GetEntities() = m_sceneSnapshot;
        m_isPlaying = false;
    }

    void Editor::ShowStatusMessage(const std::string& message, float durationSeconds) {
        m_statusMessage = message;
        m_statusTimer = durationSeconds;
    }

    void Editor::Update(Scene::Scene& scene, float delta) {
        // Count down active status banner timer
        if (m_statusTimer > 0.0f) {
            m_statusTimer -= delta;
            if (m_statusTimer < 0.0f) {
                m_statusTimer = 0.0f;
            }
        }

        // Release active slider drag when mouse button is released anywhere
        if (Input::IsMouseButtonReleased(MouseButton::Left)) {
            m_activeSliderId = -1;
            m_isDraggingEntity = false;
        }

        // Handle keyboard typing when a text input field in the Inspector is focused
        if (m_focusedTextFieldId != -1 && !m_isPlaying) {
            Entity* selected = scene.FindEntityById(m_selectedEntityId);
            if (selected) {
                std::string* targetStr = nullptr;
                if (m_focusedTextFieldId == 1) targetStr = &selected->name;
                if (m_focusedTextFieldId == 2) targetStr = &selected->text.content;
                if (m_focusedTextFieldId == 3) targetStr = &selected->sprite.texturePath;

                if (targetStr) {
                    // Append all printable characters typed during this frame
                    int ch = Input::GetCharPressed();
                    while (ch > 0) {
                        if (ch >= 32 && ch <= 126 && targetStr->size() < 64) {
                            targetStr->push_back(static_cast<char>(ch));
                        }
                        ch = Input::GetCharPressed();
                    }

                    // Handle Backspace deletion
                    if (Input::IsKeyPressed(Key::Backspace) && !targetStr->empty()) {
                        targetStr->pop_back();
                    }

                    // Unfocus on Enter
                    if (Input::IsKeyPressed(Key::Enter)) {
                        m_focusedTextFieldId = -1;
                    }
                }
            }
        }

        // In Play Mode, advance the real game simulation; in Edit Mode, allow mouse picking/dragging
        if (m_isPlaying) {
            scene.Update(delta);
        } else {
            HandleViewportInput(scene);
        }
    }

    void Editor::HandleViewportInput(Scene::Scene& scene) {
        float viewportX = LEFT_PANEL_WIDTH;
        float viewportY = TOOLBAR_HEIGHT;
        float viewportW = static_cast<float>(m_windowWidth) - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
        float viewportH = static_cast<float>(m_windowHeight) - TOOLBAR_HEIGHT;

        float mx = Input::GetMouseX();
        float my = Input::GetMouseY();

        // Only pick entities if the click occurred inside the central 2D viewport
        if (Input::IsMouseButtonPressed(MouseButton::Left) &&
            IsMouseOver(viewportX, viewportY, viewportW, viewportH)) {

            // Unfocus any active Inspector text field when clicking the viewport
            m_focusedTextFieldId = -1;

            // Iterate backwards (front-to-back visually) so top-most entities are picked first
            auto& entities = scene.GetEntities();
            bool hitAny = false;
            for (int i = static_cast<int>(entities.size()) - 1; i >= 0; --i) {
                Entity& e = entities[i];
                if (!e.active) continue;

                // Compute clickable bounds (uses text dimensions if Sprite is disabled)
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

                if (mx >= hitX && mx <= hitX + hitW && my >= hitY && my <= hitY + hitH) {
                    m_selectedEntityId = e.id;
                    m_isDraggingEntity = true;
                    m_dragOffsetX = mx - e.transform.x;
                    m_dragOffsetY = my - e.transform.y;
                    hitAny = true;
                    break;
                }
            }

            if (!hitAny) {
                m_selectedEntityId = 0;
            }
        }

        // Update position of the dragged entity while holding the left mouse button
        if (m_isDraggingEntity && Input::IsMouseButtonDown(MouseButton::Left)) {
            Entity* selected = scene.FindEntityById(m_selectedEntityId);
            if (selected) {
                selected->transform.x = mx - m_dragOffsetX;
                selected->transform.y = my - m_dragOffsetY;
            }
        }
    }

    void Editor::Render(Scene::Scene& scene) {
        // 1. Draw the subtle background reference grid in Edit Mode
        if (!m_isPlaying) {
            RenderViewportGrid();
        }

        // 2. Render all entities in the Scene
        scene.Render();

        // 3. Draw visual selection gizmo and collider indicators in Edit Mode
        if (!m_isPlaying) {
            RenderSelectionGizmo(scene);
        }

        // 4. Render the Editor UI panels on top of the viewport
        RenderEntityListPanel(scene);
        RenderInspectorPanel(scene);
        RenderTopToolbar(scene);
    }

    void Editor::RenderViewportGrid() const {
        float startX = LEFT_PANEL_WIDTH;
        float endX = static_cast<float>(m_windowWidth) - RIGHT_PANEL_WIDTH;
        float startY = TOOLBAR_HEIGHT;
        float endY = static_cast<float>(m_windowHeight);

        ColorRGBA gridColor = { 38, 39, 46, 255 };
        const float gridStep = 40.0f;

        for (float x = startX; x < endX; x += gridStep) {
            Renderer2D::DrawLine2D(x, startY, x, endY, gridColor);
        }
        for (float y = startY; y < endY; y += gridStep) {
            Renderer2D::DrawLine2D(startX, y, endX, y, gridColor);
        }
    }

    void Editor::RenderSelectionGizmo(const Scene::Scene& scene) const {
        // Draw green collision bounds for entities with active colliders
        for (const auto& e : scene.GetEntities()) {
            if (!e.active) continue;
            if (e.collider.enabled) {
                Renderer2D::DrawRectOutline(
                    e.transform.x, e.transform.y,
                    e.transform.width, e.transform.height,
                    1.0f, { 75, 220, 120, 140 }
                );
            }
        }

        // Draw bright highlight border around the currently selected entity
        const Entity* selected = const_cast<Scene::Scene&>(scene).FindEntityById(m_selectedEntityId);
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

        Renderer2D::DrawRectOutline(bx - 3.0f, by - 3.0f, bw + 6.0f, bh + 6.0f, 2.0f, { 255, 215, 60, 255 });
    }

    void Editor::RenderTopToolbar(Scene::Scene& scene) {
        float w = static_cast<float>(m_windowWidth);

        // Toolbar background and bottom divider line
        Renderer2D::DrawRect(0.0f, 0.0f, w, TOOLBAR_HEIGHT, { 20, 21, 26, 255 });
        Renderer2D::DrawLine2D(0.0f, TOOLBAR_HEIGHT, w, TOOLBAR_HEIGHT, { 55, 58, 70, 255 });

        // Engine Brand Title
        Renderer2D::DrawLabel("TALOS ENGINE", 16.0f, 13.0f, 20, { 95, 195, 255, 255 });
        Renderer2D::DrawLabel("v0.1", 168.0f, 16.0f, 16, { 130, 135, 150, 255 });

        // Instant Play / Stop Simulation Toggle Button
        float centerBtnX = (w * 0.5f) - 160.0f;
        if (!m_isPlaying) {
            if (DrawButton(centerBtnX, 8.0f, 120.0f, 30.0f, "PLAY GAME",
                           { 40, 155, 85, 255 }, { 55, 185, 105, 255 })) {
                StartPlayMode(scene);
            }

            if (m_statusTimer > 0.0f) {
                Renderer2D::DrawLabel(m_statusMessage.c_str(), centerBtnX + 132.0f, 15.0f, 16, { 95, 230, 135, 255 });
            } else {
                Renderer2D::DrawLabel("MODE: EDITOR", centerBtnX + 132.0f, 15.0f, 16, { 160, 165, 180, 255 });
            }
        } else {
            if (DrawButton(centerBtnX, 8.0f, 120.0f, 30.0f, "STOP GAME",
                           { 210, 65, 65, 255 }, { 235, 85, 85, 255 })) {
                StopPlayMode(scene);
            }
            Renderer2D::DrawLabel("MODE: PLAYING LIVE (0.0s reload)", centerBtnX + 132.0f, 15.0f, 16, { 95, 230, 135, 255 });
        }

        // Entity Management & Scene Serialization Buttons (only active in Edit Mode)
        if (!m_isPlaying) {
            if (DrawButton(LEFT_PANEL_WIDTH + 12.0f, 8.0f, 115.0f, 30.0f, "+ New Entity",
                           { 55, 115, 200, 255 }, { 75, 140, 230, 255 })) {
                char nameBuf[32];
                std::snprintf(nameBuf, sizeof(nameBuf), "Object_%d", m_newEntityCounter++);
                Entity& created = scene.CreateEntity(nameBuf, 480.0f, 320.0f);
                created.sprite.color = PRESET_COLORS[m_newEntityCounter % NUM_PRESET_COLORS];
                m_selectedEntityId = created.id;
            }

            if (m_selectedEntityId != 0) {
                if (DrawButton(LEFT_PANEL_WIDTH + 136.0f, 8.0f, 85.0f, 30.0f, "Delete",
                               { 140, 50, 55, 255 }, { 180, 65, 70, 255 })) {
                    scene.RemoveEntity(m_selectedEntityId);
                    m_selectedEntityId = 0;
                }
            }

            // Right-side Scene Save & Load buttons
            float rightActionsX = w - RIGHT_PANEL_WIDTH - 228.0f;
            if (DrawButton(rightActionsX, 8.0f, 104.0f, 30.0f, "Save Scene",
                           { 48, 125, 115, 255 }, { 65, 155, 142, 255 })) {
                if (SceneSerializer::Save(scene, m_sceneFilePath)) {
                    ShowStatusMessage("Saved -> " + m_sceneFilePath);
                } else {
                    ShowStatusMessage("Save failed!");
                }
            }

            if (DrawButton(rightActionsX + 112.0f, 8.0f, 104.0f, 30.0f, "Load Scene",
                           { 70, 75, 95, 255 }, { 92, 98, 122, 255 })) {
                if (SceneSerializer::Load(scene, m_sceneFilePath)) {
                    m_selectedEntityId = scene.GetEntities().empty() ? 0 : scene.GetEntities().front().id;
                    ShowStatusMessage("Loaded <- " + m_sceneFilePath);
                } else {
                    ShowStatusMessage("No saved scene found!");
                }
            }
        }
    }

    void Editor::RenderEntityListPanel(Scene::Scene& scene) {
        float h = static_cast<float>(m_windowHeight) - TOOLBAR_HEIGHT;

        // Left Panel Background
        Renderer2D::DrawRect(0.0f, TOOLBAR_HEIGHT, LEFT_PANEL_WIDTH, h, { 24, 25, 31, 255 });
        Renderer2D::DrawLine2D(LEFT_PANEL_WIDTH, TOOLBAR_HEIGHT, LEFT_PANEL_WIDTH, static_cast<float>(m_windowHeight), { 55, 58, 70, 255 });

        Renderer2D::DrawLabel("SCENE ENTITIES", 14.0f, TOOLBAR_HEIGHT + 14.0f, 16, { 150, 155, 175, 255 });
        Renderer2D::DrawLine2D(12.0f, TOOLBAR_HEIGHT + 36.0f, LEFT_PANEL_WIDTH - 12.0f, TOOLBAR_HEIGHT + 36.0f, { 45, 48, 58, 255 });

        float rowY = TOOLBAR_HEIGHT + 46.0f;
        for (const auto& entity : scene.GetEntities()) {
            bool isSelected = (entity.id == m_selectedEntityId);
            bool isHovered = IsMouseOver(8.0f, rowY, LEFT_PANEL_WIDTH - 16.0f, 30.0f);

            ColorRGBA rowBg = isSelected ? ColorRGBA{ 52, 95, 160, 255 }
                            : (isHovered ? ColorRGBA{ 38, 41, 52, 255 } : ColorRGBA{ 29, 31, 38, 255 });

            Renderer2D::DrawRect(8.0f, rowY, LEFT_PANEL_WIDTH - 16.0f, 30.0f, rowBg);

            // Render a 16x16 texture thumbnail if the entity has a texture assigned;
            // otherwise render a 16x16 solid color indicator swatch.
            if (entity.sprite.enabled && !entity.sprite.texturePath.empty()) {
                Renderer2D::DrawSprite(entity.sprite.texturePath.c_str(), 16.0f, rowY + 7.0f, 16.0f, 16.0f, entity.sprite.color);
            } else {
                ColorRGBA swatch = entity.sprite.enabled ? entity.sprite.color : entity.text.color;
                Renderer2D::DrawRect(16.0f, rowY + 7.0f, 16.0f, 16.0f, swatch);
            }

            // Entity Name
            Renderer2D::DrawLabel(entity.name.c_str(), 40.0f, rowY + 7.0f, 16, { 235, 235, 240, 255 });

            // Click to select entity
            if (isHovered && Input::IsMouseButtonPressed(MouseButton::Left)) {
                m_selectedEntityId = entity.id;
                m_focusedTextFieldId = -1;
            }

            rowY += 34.0f;
        }
    }

    void Editor::RenderInspectorPanel(Scene::Scene& scene) {
        float panelX = static_cast<float>(m_windowWidth) - RIGHT_PANEL_WIDTH;
        float panelH = static_cast<float>(m_windowHeight) - TOOLBAR_HEIGHT;

        // Right Panel Background
        Renderer2D::DrawRect(panelX, TOOLBAR_HEIGHT, RIGHT_PANEL_WIDTH, panelH, { 24, 25, 31, 255 });
        Renderer2D::DrawLine2D(panelX, TOOLBAR_HEIGHT, panelX, static_cast<float>(m_windowHeight), { 55, 58, 70, 255 });

        Renderer2D::DrawLabel("FIRST-LEVEL INSPECTOR", panelX + 16.0f, TOOLBAR_HEIGHT + 14.0f, 16, { 150, 155, 175, 255 });
        Renderer2D::DrawLine2D(panelX + 12.0f, TOOLBAR_HEIGHT + 36.0f, panelX + RIGHT_PANEL_WIDTH - 12.0f, TOOLBAR_HEIGHT + 36.0f, { 45, 48, 58, 255 });

        Entity* selected = scene.FindEntityById(m_selectedEntityId);
        if (!selected) {
            Renderer2D::DrawLabel("Select any entity in the viewport", panelX + 20.0f, TOOLBAR_HEIGHT + 65.0f, 16, { 120, 125, 140, 255 });
            Renderer2D::DrawLabel("or from the left panel to edit its", panelX + 20.0f, TOOLBAR_HEIGHT + 88.0f, 16, { 120, 125, 140, 255 });
            Renderer2D::DrawLabel("properties directly.", panelX + 20.0f, TOOLBAR_HEIGHT + 111.0f, 16, { 120, 125, 140, 255 });
            return;
        }

        float x = panelX + 16.0f;
        float w = RIGHT_PANEL_WIDTH - 32.0f;
        float y = TOOLBAR_HEIGHT + 44.0f;

        // 1. Entity Identity & Master Active Toggle
        DrawCheckbox(x, y, "Active", selected->active);
        y += 24.0f;
        DrawTextInputBox(x, y, w, "Name:", selected->name, 1);
        y += 32.0f;

        // 2. Transform Component (Always accessible on first level)
        Renderer2D::DrawLabel("TRANSFORM 2D", x, y, 15, { 95, 195, 255, 255 });
        y += 18.0f;
        DrawFloatSlider(x, y, w, "Pos X", selected->transform.x, 230.0f, 980.0f, 10);
        y += 22.0f;
        DrawFloatSlider(x, y, w, "Pos Y", selected->transform.y, 50.0f, 740.0f, 11);
        y += 22.0f;
        DrawFloatSlider(x, y, w, "Width", selected->transform.width, 16.0f, 500.0f, 12);
        y += 22.0f;
        DrawFloatSlider(x, y, w, "Height", selected->transform.height, 16.0f, 500.0f, 13);
        y += 28.0f;

        // 3. Sprite Component (2D Texture / Solid Box + Color / Tint)
        DrawCheckbox(x, y, "SPRITE (2D Image / Box)", selected->sprite.enabled);
        if (selected->sprite.enabled) {
            // Live status indicator showing whether the entity uses a solid box or a 2D texture
            if (selected->sprite.texturePath.empty()) {
                Renderer2D::DrawLabel("[Solid Box]", x + w - 74.0f, y + 2.0f, 14, { 135, 140, 155, 255 });
            } else if (Renderer2D::HasTexture(selected->sprite.texturePath.c_str())) {
                Renderer2D::DrawLabel("[Texture OK]", x + w - 82.0f, y + 2.0f, 14, { 90, 225, 135, 255 });
            } else {
                Renderer2D::DrawLabel("[Missing!]", x + w - 68.0f, y + 2.0f, 14, { 245, 95, 95, 255 });
            }
        }
        y += 24.0f;

        if (selected->sprite.enabled) {
            DrawTextureSelector(x, y, w, selected->sprite);
            y += 58.0f;

            const char* colorLabel = selected->sprite.texturePath.empty() ? "Box Color" : "Img Tint";
            DrawColorPickerRow(x, y, w, colorLabel, selected->sprite.color, 20);
            y += 82.0f;
        }

        // 4. Text Component (Solves Godot's 3-nested-menus complaint: Text, Size & Color right here!)
        DrawCheckbox(x, y, "TEXT LABEL (First-Level)", selected->text.enabled);
        y += 24.0f;
        if (selected->text.enabled) {
            DrawTextInputBox(x, y, w, "Text:", selected->text.content, 2);
            y += 28.0f;
            DrawIntSlider(x, y, w, "Font Size", selected->text.fontSize, 12, 56, 30);
            y += 22.0f;
            DrawFloatSlider(x, y, w, "Offset Y", selected->text.offsetY, -80.0f, 80.0f, 31);
            y += 24.0f;
            DrawColorPickerRow(x, y, w, "Font Color", selected->text.color, 40);
            y += 82.0f;
        }

        // 5. Collider 2D Component
        DrawCheckbox(x, y, "COLLIDER 2D (Solid Box)", selected->collider.enabled);
        y += 24.0f;

        // 6. Movement 2D Component
        DrawCheckbox(x, y, "MOVEMENT (WASD Control)", selected->movement.enabled);
        y += 22.0f;
        if (selected->movement.enabled) {
            DrawCheckbox(x + 14.0f, y, "Player Controlled", selected->movement.playerControlled);
            y += 22.0f;
            DrawFloatSlider(x, y, w, "Speed", selected->movement.speed, 50.0f, 700.0f, 50);
        }
    }

    // =========================================================================
    // Immediate-Mode UI Widget Implementations
    // =========================================================================

    bool Editor::DrawButton(float x, float y, float w, float h, const char* label,
                            ColorRGBA bgColor, ColorRGBA hoverColor) {
        bool hovered = IsMouseOver(x, y, w, h);
        Renderer2D::DrawRect(x, y, w, h, hovered ? hoverColor : bgColor);
        Renderer2D::DrawRectOutline(x, y, w, h, 1.0f, { 255, 255, 255, 45 });

        int textW = Renderer2D::MeasureTextWidth(label, 15);
        float textX = x + (w - static_cast<float>(textW)) * 0.5f;
        float textY = y + (h - 15.0f) * 0.5f;
        Renderer2D::DrawLabel(label, textX, textY, 15, { 250, 250, 255, 255 });

        return hovered && Input::IsMouseButtonPressed(MouseButton::Left);
    }

    bool Editor::DrawCheckbox(float x, float y, const char* label, bool& value) {
        float boxSize = 18.0f;
        bool hovered = IsMouseOver(x, y, boxSize + 190.0f, boxSize);

        Renderer2D::DrawRect(x, y, boxSize, boxSize, { 40, 43, 54, 255 });
        Renderer2D::DrawRectOutline(x, y, boxSize, boxSize, 1.0f, { 95, 100, 120, 255 });

        if (value) {
            Renderer2D::DrawRect(x + 4.0f, y + 4.0f, boxSize - 8.0f, boxSize - 8.0f, { 80, 190, 255, 255 });
        }

        Renderer2D::DrawLabel(label, x + boxSize + 8.0f, y + 1.0f, 15,
                              value ? ColorRGBA{ 240, 240, 245, 255 } : ColorRGBA{ 150, 155, 170, 255 });

        if (hovered && Input::IsMouseButtonPressed(MouseButton::Left)) {
            value = !value;
            m_focusedTextFieldId = -1;
            return true;
        }
        return false;
    }

    void Editor::DrawFloatSlider(float x, float y, float w, const char* label,
                                 float& value, float minVal, float maxVal, int sliderId) {
        Renderer2D::DrawLabel(label, x, y + 1.0f, 14, { 185, 190, 205, 255 });

        float barX = x + 84.0f;
        float barW = w - 84.0f;
        float barH = 17.0f;

        if (IsMouseOver(barX, y, barW, barH) && Input::IsMouseButtonPressed(MouseButton::Left)) {
            m_activeSliderId = sliderId;
            m_focusedTextFieldId = -1;
        }

        if (m_activeSliderId == sliderId && Input::IsMouseButtonDown(MouseButton::Left)) {
            float t = (Input::GetMouseX() - barX) / barW;
            t = std::max(0.0f, std::min(1.0f, t));
            value = minVal + t * (maxVal - minVal);
        }

        float fillRatio = (value - minVal) / (maxVal - minVal);
        fillRatio = std::max(0.0f, std::min(1.0f, fillRatio));

        Renderer2D::DrawRect(barX, y, barW, barH, { 36, 38, 48, 255 });
        Renderer2D::DrawRect(barX, y, barW * fillRatio, barH, { 65, 135, 220, 255 });
        Renderer2D::DrawRectOutline(barX, y, barW, barH, 1.0f, { 70, 75, 90, 255 });

        char valBuf[32];
        std::snprintf(valBuf, sizeof(valBuf), "%.0f", value);
        Renderer2D::DrawLabel(valBuf, barX + 8.0f, y + 1.0f, 14, { 245, 245, 250, 255 });
    }

    void Editor::DrawIntSlider(float x, float y, float w, const char* label,
                               int& value, int minVal, int maxVal, int sliderId) {
        float floatVal = static_cast<float>(value);
        DrawFloatSlider(x, y, w, label, floatVal, static_cast<float>(minVal), static_cast<float>(maxVal), sliderId);
        value = static_cast<int>(floatVal + 0.5f);
    }

    void Editor::DrawTextureSelector(float x, float y, float w, Scene::SpriteComponent& sprite) {
        // 1. One-Click Built-In Texture Preset Buttons
        Renderer2D::DrawLabel("Preset:", x, y + 4.0f, 14, { 185, 190, 205, 255 });

        float btnStartX = x + 54.0f;
        float availW = w - 54.0f;
        float gap = 4.0f;
        float btnW = (availW - gap * (NUM_TEXTURE_PRESETS - 1)) / static_cast<float>(NUM_TEXTURE_PRESETS);

        for (int i = 0; i < NUM_TEXTURE_PRESETS; ++i) {
            float bx = btnStartX + i * (btnW + gap);
            bool isCurrent = (sprite.texturePath == TEXTURE_PRESETS[i].path);

            ColorRGBA bg = isCurrent ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 };
            ColorRGBA hover = isCurrent ? ColorRGBA{ 68, 135, 220, 255 } : ColorRGBA{ 54, 58, 74, 255 };

            if (DrawButton(bx, y, btnW, 22.0f, TEXTURE_PRESETS[i].label, bg, hover)) {
                bool wasSolidBox = sprite.texturePath.empty();
                sprite.texturePath = TEXTURE_PRESETS[i].path;
                m_focusedTextFieldId = -1;

                // When switching from a solid colored box to a 2D image preset, automatically
                // set the tint to pure white (255,255,255) so the texture displays its true colors.
                if (wasSolidBox && !sprite.texturePath.empty()) {
                    sprite.color = { 255, 255, 255, 255 };
                } else if (!wasSolidBox && sprite.texturePath.empty() &&
                           sprite.color.r == 255 && sprite.color.g == 255 && sprite.color.b == 255) {
                    sprite.color = { 80, 180, 250, 255 };
                }
            }
        }

        // 2. Direct Editable File Path Input Box ("Image:")
        DrawTextInputBox(x, y + 27.0f, w, "Image:", sprite.texturePath, 3);
    }

    void Editor::DrawColorPickerRow(float x, float y, float w, const char* label,
                                    ColorRGBA& color, int baseSliderId) {
        Renderer2D::DrawLabel(label, x, y + 2.0f, 14, { 185, 190, 205, 255 });

        // Current color preview box
        Renderer2D::DrawRect(x + 84.0f, y, 22.0f, 19.0f, color);
        Renderer2D::DrawRectOutline(x + 84.0f, y, 22.0f, 19.0f, 1.0f, { 220, 220, 230, 255 });

        // 8 One-Click Preset Swatches right on the first level
        float swatchX = x + 114.0f;
        float swatchSize = 22.0f;
        for (int i = 0; i < NUM_PRESET_COLORS; ++i) {
            Renderer2D::DrawRect(swatchX, y, swatchSize, 19.0f, PRESET_COLORS[i]);
            bool hovered = IsMouseOver(swatchX, y, swatchSize, 19.0f);
            if (hovered) {
                Renderer2D::DrawRectOutline(swatchX, y, swatchSize, 19.0f, 2.0f, { 255, 255, 255, 255 });
                if (Input::IsMouseButtonPressed(MouseButton::Left)) {
                    color = PRESET_COLORS[i];
                }
            }
            swatchX += swatchSize + 4.0f;
        }

        // Direct R, G, B Sliders underneath for fine tuning
        float r = static_cast<float>(color.r);
        float g = static_cast<float>(color.g);
        float b = static_cast<float>(color.b);

        DrawFloatSlider(x, y + 23.0f, w, "Red (R)",   r, 0.0f, 255.0f, baseSliderId + 1);
        DrawFloatSlider(x, y + 42.0f, w, "Green (G)", g, 0.0f, 255.0f, baseSliderId + 2);
        DrawFloatSlider(x, y + 61.0f, w, "Blue (B)",  b, 0.0f, 255.0f, baseSliderId + 3);

        color.r = static_cast<uint8_t>(r);
        color.g = static_cast<uint8_t>(g);
        color.b = static_cast<uint8_t>(b);
    }

    void Editor::DrawTextInputBox(float x, float y, float w, const char* label,
                                  std::string& textBuffer, int fieldId) {
        Renderer2D::DrawLabel(label, x, y + 4.0f, 14, { 185, 190, 205, 255 });

        float boxX = x + 54.0f;
        float boxW = w - 54.0f;
        float boxH = 23.0f;
        bool isFocused = (m_focusedTextFieldId == fieldId);
        bool hovered = IsMouseOver(boxX, y, boxW, boxH);

        if (hovered && Input::IsMouseButtonPressed(MouseButton::Left)) {
            m_focusedTextFieldId = fieldId;
        }

        Renderer2D::DrawRect(boxX, y, boxW, boxH, isFocused ? ColorRGBA{ 45, 50, 66, 255 } : ColorRGBA{ 34, 36, 46, 255 });
        Renderer2D::DrawRectOutline(boxX, y, boxW, boxH, isFocused ? 2.0f : 1.0f,
                                    isFocused ? ColorRGBA{ 95, 195, 255, 255 } : ColorRGBA{ 75, 80, 98, 255 });

        std::string displayStr = isFocused ? (textBuffer + "|") : textBuffer;
        if (!isFocused && displayStr.empty() && fieldId == 3) {
            Renderer2D::DrawLabel("(none - solid color box)", boxX + 8.0f, y + 4.0f, 14, { 110, 115, 130, 255 });
        } else {
            Renderer2D::DrawLabel(displayStr.c_str(), boxX + 8.0f, y + 4.0f, 14, { 245, 245, 250, 255 });
        }
    }

}
