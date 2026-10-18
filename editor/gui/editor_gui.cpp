#include "editor/gui/editor_gui.h"
#include "core/input.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Scene;

    const ColorRGBA EditorGui::PRESET_COLORS[EditorGui::NUM_PRESET_COLORS] = {
        { 80,  180, 250, 255 }, // Sky Blue
        { 235, 95,  95,  255 }, // Coral Red
        { 90,  215, 130, 255 }, // Emerald Green
        { 250, 200, 75,  255 }, // Warm Gold
        { 185, 115, 250, 255 }, // Vivid Purple
        { 250, 140, 70,  255 }, // Tangerine Orange
        { 255, 255, 255, 255 }, // Pure White (Unmodified Texture Tint)
        { 60,  65,  80,  255 }  // Slate Gray
    };

    const TexturePreset EditorGui::TEXTURE_PRESETS[EditorGui::NUM_TEXTURE_PRESETS] = {
        { "None",  "" },
        { "Hero",  "examples/assets/textures/hero.png" },
        { "Sheet", "examples/assets/textures/sheet.png" },
        { "Crate", "examples/assets/textures/crate.png" },
        { "Brick", "examples/assets/textures/brick.png" },
        { "Coin",  "examples/assets/textures/coin.png" }
    };

    namespace {

        bool IsWordChar(char c) {
            return (c >= 'a' && c <= 'z') ||
                   (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') ||
                   (c == '_');
        }

        int FindNextWordBoundary(const std::string& text, int currentPos, int dir) {
            int len = static_cast<int>(text.size());
            int pos = std::max(0, std::min(currentPos, len));
            if (dir < 0) {
                while (pos > 0 && !IsWordChar(text[pos - 1])) {
                    --pos;
                }
                while (pos > 0 && IsWordChar(text[pos - 1])) {
                    --pos;
                }
            } else if (dir > 0) {
                while (pos < len && !IsWordChar(text[pos])) {
                    ++pos;
                }
                while (pos < len && IsWordChar(text[pos])) {
                    ++pos;
                }
            }
            return pos;
        }

        int GetTextCharIndexAtMouseX(const std::string& text, float textStartX, float mouseX, int fontSize) {
            float relX = mouseX - textStartX;
            if (relX <= 0.0f || text.empty()) {
                return 0;
            }
            int len = static_cast<int>(text.size());
            int prevWidth = 0;
            for (int i = 1; i <= len; ++i) {
                std::string prefix = text.substr(0, static_cast<size_t>(i));
                int curWidth = Renderer2D::MeasureTextWidth(prefix.c_str(), fontSize);
                float charMidpoint = (static_cast<float>(prevWidth) + static_cast<float>(curWidth)) * 0.5f;
                if (relX < charMidpoint) {
                    return i - 1;
                }
                prevWidth = curWidth;
            }
            return len;
        }

    } // anonymous namespace

    void EditorGui::SetFrameContext(Scene::Scene* activeScene,
                                    EditorUndoRedo* undoRedo,
                                    Scene::EntityID selectedEntityId,
                                    bool isPlaying) {
        m_activeScene      = activeScene;
        m_undoRedo         = undoRedo;
        m_selectedEntityId = selectedEntityId;
        m_isPlaying        = isPlaying;
    }

    void EditorGui::OnLeftMouseReleased() {
        m_activeSliderId = -1;
        m_isDraggingText = false;
    }

    void EditorGui::PushUndoIfActive() {
        if (m_activeScene && m_undoRedo) {
            m_undoRedo->PushUndoState(
                *m_activeScene,
                m_selectedEntityId,
                m_textCursorPos,
                m_textSelectionAnchor,
                m_isPlaying
            );
        }
    }

    void EditorGui::BeginContinuousIfActive() {
        if (m_activeScene && m_undoRedo) {
            m_undoRedo->BeginContinuousEdit(
                *m_activeScene,
                m_selectedEntityId,
                m_textCursorPos,
                m_textSelectionAnchor,
                m_isPlaying
            );
        }
    }

    bool EditorGui::IsMouseOver(float x, float y, float w, float h) {
        float mx = Input::GetMouseX();
        float my = Input::GetMouseY();
        return (mx >= x && mx <= x + w && my >= y && my <= y + h);
    }

    void EditorGui::UpdateFocusedTextField(Scene::Scene& scene,
                                           Scene::EntityID selectedEntityId,
                                           std::string& field4Buffer,
                                           bool isPlaying,
                                           bool ctrlDown,
                                           bool shiftDown,
                                           bool& outEnterPressedOnField4) {
        outEnterPressedOnField4 = false;
        if (m_focusedTextFieldId == -1 || isPlaying) return;

        Entity* selected = scene.FindEntityById(selectedEntityId);
        std::string* targetStr = nullptr;
        bool isEntityField = false;

        if (m_focusedTextFieldId == 4) {
            targetStr = &field4Buffer;
            isEntityField = false;
        } else if (selected) {
            if (m_focusedTextFieldId == 1) { targetStr = &selected->name; isEntityField = true; }
            if (m_focusedTextFieldId == 2) { targetStr = &selected->text.content; isEntityField = true; }
            if (m_focusedTextFieldId == 3) { targetStr = &selected->sprite.texturePath; isEntityField = true; }
            if (m_focusedTextFieldId == 5) { targetStr = &selected->camera.targetName; isEntityField = true; }
        }

        if (!targetStr) return;

        int len = static_cast<int>(targetStr->size());
        m_textCursorPos = std::max(0, std::min(m_textCursorPos, len));
        m_textSelectionAnchor = std::max(0, std::min(m_textSelectionAnchor, len));

        auto hasSelection = [&]() { return m_textCursorPos != m_textSelectionAnchor; };
        auto getSelStart  = [&]() { return std::min(m_textCursorPos, m_textSelectionAnchor); };
        auto getSelEnd    = [&]() { return std::max(m_textCursorPos, m_textSelectionAnchor); };

        auto deleteSelection = [&]() {
            int s = getSelStart();
            int e = getSelEnd();
            targetStr->erase(static_cast<size_t>(s), static_cast<size_t>(e - s));
            m_textCursorPos = s;
            m_textSelectionAnchor = s;
        };

        // 0. Select All shortcut (Ctrl + A) inside the active text box
        if (ctrlDown && Input::IsKeyPressed(Key::A)) {
            m_textSelectionAnchor = 0;
            m_textCursorPos = len;
        }

        // 1. Left / Right Arrow Navigation (supports Shift selection & Ctrl word-by-word jump)
        bool leftPressed  = Input::IsKeyPressed(Key::Left);
        bool rightPressed = Input::IsKeyPressed(Key::Right);

        if (leftPressed || rightPressed) {
            int dir = leftPressed ? -1 : 1;

            if (!shiftDown && hasSelection() && !ctrlDown) {
                m_textCursorPos = (dir < 0) ? getSelStart() : getSelEnd();
                m_textSelectionAnchor = m_textCursorPos;
            } else {
                if (ctrlDown) {
                    m_textCursorPos = FindNextWordBoundary(*targetStr, m_textCursorPos, dir);
                } else {
                    m_textCursorPos = std::max(0, std::min(m_textCursorPos + dir, len));
                }

                if (!shiftDown) {
                    m_textSelectionAnchor = m_textCursorPos;
                }
            }
        }

        // 2. Printable character insertion at cursor (replaces selection if any)
        if (!ctrlDown) {
            int ch = Input::GetCharPressed();
            while (ch > 0) {
                if (ch >= 32 && ch <= 126) {
                    char chToInsert = static_cast<char>(ch);
                    if (m_focusedTextFieldId == 4 && chToInsert == ' ') {
                        chToInsert = '_';
                    }

                    if (hasSelection() || targetStr->size() < 64) {
                        if (isEntityField) PushUndoIfActive();
                        if (hasSelection()) {
                            deleteSelection();
                        }
                        targetStr->insert(static_cast<size_t>(m_textCursorPos), 1, chToInsert);
                        ++m_textCursorPos;
                        m_textSelectionAnchor = m_textCursorPos;
                    }
                }
                ch = Input::GetCharPressed();
            }
        }

        // 3. Backspace & Delete (deletes active selection, single character, or whole word with Ctrl)
        if (Input::IsKeyPressed(Key::Backspace)) {
            if (hasSelection()) {
                if (isEntityField) PushUndoIfActive();
                deleteSelection();
            } else if (m_textCursorPos > 0) {
                if (isEntityField) PushUndoIfActive();
                int eraseStart = ctrlDown ? FindNextWordBoundary(*targetStr, m_textCursorPos, -1)
                                          : (m_textCursorPos - 1);
                targetStr->erase(static_cast<size_t>(eraseStart),
                                 static_cast<size_t>(m_textCursorPos - eraseStart));
                m_textCursorPos = eraseStart;
                m_textSelectionAnchor = m_textCursorPos;
            }
        }

        if (Input::IsKeyPressed(Key::Delete)) {
            int curLen = static_cast<int>(targetStr->size());
            if (hasSelection()) {
                if (isEntityField) PushUndoIfActive();
                deleteSelection();
            } else if (m_textCursorPos < curLen) {
                if (isEntityField) PushUndoIfActive();
                int eraseEnd = ctrlDown ? FindNextWordBoundary(*targetStr, m_textCursorPos, 1)
                                        : (m_textCursorPos + 1);
                targetStr->erase(static_cast<size_t>(m_textCursorPos),
                                 static_cast<size_t>(eraseEnd - m_textCursorPos));
                m_textSelectionAnchor = m_textCursorPos;
            }
        }

        // 4. Submit or Unfocus on Enter / Escape
        if (Input::IsKeyPressed(Key::Enter)) {
            if (m_focusedTextFieldId == 4 && !field4Buffer.empty()) {
                outEnterPressedOnField4 = true;
            } else {
                UnfocusTextField();
            }
        } else if (Input::IsKeyPressed(Key::Escape)) {
            UnfocusTextField();
        }
    }

    bool EditorGui::DrawButton(float x, float y, float w, float h, const char* label,
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

    bool EditorGui::DrawCheckbox(float x, float y, const char* label, bool& value) {
        float boxSize = 18.0f;
        float hitW = boxSize + 10.0f + static_cast<float>(Renderer2D::MeasureTextWidth(label, 15));
        bool hovered = IsMouseOver(x, y, hitW, boxSize);

        Renderer2D::DrawRect(x, y, boxSize, boxSize, { 40, 43, 54, 255 });
        Renderer2D::DrawRectOutline(x, y, boxSize, boxSize, 1.0f, { 95, 100, 120, 255 });

        if (value) {
            Renderer2D::DrawRect(x + 4.0f, y + 4.0f, boxSize - 8.0f, boxSize - 8.0f, { 80, 190, 255, 255 });
        }

        Renderer2D::DrawLabel(label, x + boxSize + 8.0f, y + 1.0f, 15,
                              value ? ColorRGBA{ 240, 240, 245, 255 } : ColorRGBA{ 150, 155, 170, 255 });

        if (hovered && Input::IsMouseButtonPressed(MouseButton::Left)) {
            PushUndoIfActive();
            value = !value;
            m_focusedTextFieldId = -1;
            return true;
        }
        return false;
    }

    void EditorGui::DrawFloatSlider(float x, float y, float w, const char* label,
                                    float& value, float minVal, float maxVal, int sliderId) {
        Renderer2D::DrawLabel(label, x, y + 1.0f, 14, { 185, 190, 205, 255 });

        float barX = x + 84.0f;
        float barW = w - 84.0f;
        float barH = 17.0f;

        if (IsMouseOver(barX, y, barW, barH) && Input::IsMouseButtonPressed(MouseButton::Left)) {
            BeginContinuousIfActive();
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

    void EditorGui::DrawIntSlider(float x, float y, float w, const char* label,
                                  int& value, int minVal, int maxVal, int sliderId) {
        float floatVal = static_cast<float>(value);
        DrawFloatSlider(x, y, w, label, floatVal, static_cast<float>(minVal), static_cast<float>(maxVal), sliderId);
        value = static_cast<int>(std::round(floatVal));
    }

    void EditorGui::DrawSortingControls(float x, float y, float w, int& sortingLayer, int& zOrder, int zSliderId) {
        Renderer2D::DrawLabel("Layer:", x, y + 3.0f, 14, { 185, 190, 205, 255 });

        float btnStartX = x + 54.0f;
        float availW = w - 54.0f;
        float gap = 4.0f;
        float btnW = (availW - gap * (SortingLayer::Count - 1)) / static_cast<float>(SortingLayer::Count);

        for (int i = 0; i < SortingLayer::Count; ++i) {
            float bx = btnStartX + i * (btnW + gap);
            bool isCurrent = (sortingLayer == i);

            ColorRGBA bg = isCurrent ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 };
            ColorRGBA hover = isCurrent ? ColorRGBA{ 68, 135, 220, 255 } : ColorRGBA{ 54, 58, 74, 255 };

            if (DrawButton(bx, y, btnW, 21.0f, SortingLayer::GetShortName(i), bg, hover)) {
                if (!isCurrent) {
                    PushUndoIfActive();
                }
                sortingLayer = i;
                m_focusedTextFieldId = -1;
            }
        }

        // Z-Order row: label + [-] step button + slider (-100..100) + [+] step button
        float zy = y + 25.0f;
        Renderer2D::DrawLabel("Z-Order", x, zy + 2.0f, 14, { 185, 190, 205, 255 });

        float minusX = x + 64.0f;
        float stepBtnW = 22.0f;
        float barH = 18.0f;

        if (DrawButton(minusX, zy, stepBtnW, barH, "-", { 42, 46, 58, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            zOrder = std::max(-100, zOrder - 1);
            m_focusedTextFieldId = -1;
        }

        float plusX = x + w - stepBtnW;
        if (DrawButton(plusX, zy, stepBtnW, barH, "+", { 42, 46, 58, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            zOrder = std::min(100, zOrder + 1);
            m_focusedTextFieldId = -1;
        }

        float barX = minusX + stepBtnW + 4.0f;
        float barW = plusX - barX - 4.0f;

        if (IsMouseOver(barX, zy, barW, barH) && Input::IsMouseButtonPressed(MouseButton::Left)) {
            BeginContinuousIfActive();
            m_activeSliderId = zSliderId;
            m_focusedTextFieldId = -1;
        }

        if (m_activeSliderId == zSliderId && Input::IsMouseButtonDown(MouseButton::Left)) {
            float t = (Input::GetMouseX() - barX) / barW;
            t = std::max(0.0f, std::min(1.0f, t));
            zOrder = static_cast<int>(std::round(-100.0f + t * 200.0f));
        }

        float fillRatio = (static_cast<float>(zOrder) + 100.0f) / 200.0f;
        fillRatio = std::max(0.0f, std::min(1.0f, fillRatio));

        Renderer2D::DrawRect(barX, zy, barW, barH, { 36, 38, 48, 255 });
        Renderer2D::DrawRect(barX, zy, barW * fillRatio, barH, { 65, 135, 220, 255 });
        Renderer2D::DrawRectOutline(barX, zy, barW, barH, 1.0f, { 70, 75, 90, 255 });

        char zBuf[32];
        std::snprintf(zBuf, sizeof(zBuf), "%d", zOrder);
        Renderer2D::DrawLabel(zBuf, barX + 8.0f, zy + 2.0f, 14, { 245, 245, 250, 255 });
    }

    void EditorGui::DrawTextureSelector(float x, float y, float w, Scene::SpriteComponent& sprite) {
        Renderer2D::DrawLabel("Preset:", x, y + 4.0f, 14, { 185, 190, 205, 255 });

        float btnStartX = x + 54.0f;
        float availW = w - 54.0f;
        float gap = 3.0f;
        float btnW = (availW - gap * (NUM_TEXTURE_PRESETS - 1)) / static_cast<float>(NUM_TEXTURE_PRESETS);

        for (int i = 0; i < NUM_TEXTURE_PRESETS; ++i) {
            float bx = btnStartX + i * (btnW + gap);
            bool isCurrent = (sprite.texturePath == TEXTURE_PRESETS[i].path);

            ColorRGBA bg = isCurrent ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 };
            ColorRGBA hover = isCurrent ? ColorRGBA{ 68, 135, 220, 255 } : ColorRGBA{ 54, 58, 74, 255 };

            if (DrawButton(bx, y, btnW, 22.0f, TEXTURE_PRESETS[i].label, bg, hover)) {
                if (!isCurrent) {
                    PushUndoIfActive();
                }

                bool wasSolidBox = sprite.texturePath.empty();
                sprite.texturePath = TEXTURE_PRESETS[i].path;
                m_focusedTextFieldId = -1;

                // When selecting the 256x64 4-frame sprite sheet preset, default to a 4-frame 64x64 animation
                if (std::string(TEXTURE_PRESETS[i].label) == "Sheet") {
                    sprite.sourceX = 0.0f;
                    sprite.sourceY = 0.0f;
                    sprite.sourceW = 64.0f;
                    sprite.sourceH = 64.0f;
                    sprite.frameCount = 4;
                    sprite.currentFrame = 0;
                    if (sprite.fps <= 0.0f) sprite.fps = 8.0f;
                    sprite.loop = true;
                    sprite.isPlaying = true;
                    sprite.frameTimer = 0.0f;
                } else {
                    if (sprite.frameCount > 1) {
                        sprite.frameCount = 1;
                        sprite.currentFrame = 0;
                        sprite.isPlaying = false;
                        sprite.frameTimer = 0.0f;
                        sprite.sourceX = 0.0f;
                        sprite.sourceY = 0.0f;
                        sprite.sourceW = 0.0f;
                        sprite.sourceH = 0.0f;
                    } else {
                        int texW = 0, texH = 0;
                        if (Renderer2D::GetTextureSize(sprite.texturePath.c_str(), texW, texH) &&
                            sprite.sourceX >= static_cast<float>(texW)) {
                            sprite.sourceX = 0.0f;
                        }
                    }
                }

                if (wasSolidBox && !sprite.texturePath.empty()) {
                    sprite.color = { 255, 255, 255, 255 };
                } else if (!wasSolidBox && sprite.texturePath.empty() &&
                           sprite.color.r == 255 && sprite.color.g == 255 && sprite.color.b == 255) {
                    sprite.color = { 80, 180, 250, 255 };
                }
            }
        }

        DrawTextInputBox(x, y + 27.0f, w, "Image:", sprite.texturePath, 3);
    }

    void EditorGui::DrawSpriteRegionControls(float x, float y, float w, Scene::SpriteComponent& sprite, int baseSliderId) {
        // 1. Horizontal & Vertical Mirroring Toggles (Flip X / Flip Y)
        DrawCheckbox(x, y, "Flip X (Horiz)", sprite.flipX);
        DrawCheckbox(x + w * 0.52f, y, "Flip Y (Vert)", sprite.flipY);

        // 2. UV Sub-Rectangle Header + Quick Crop / Frame Step Buttons
        float uvHeaderY = y + 23.0f;
        int texW = 0, texH = 0;
        bool hasTex = Renderer2D::GetTextureSize(sprite.texturePath.c_str(), texW, texH);
        float maxW = (hasTex && texW > 0) ? static_cast<float>(texW) : 256.0f;
        float maxH = (hasTex && texH > 0) ? static_cast<float>(texH) : 256.0f;

        char headerLabel[48];
        if (hasTex) {
            std::snprintf(headerLabel, sizeof(headerLabel), "UV Sub-Rect (%dx%d)", texW, texH);
        } else {
            std::snprintf(headerLabel, sizeof(headerLabel), "UV Sub-Rect (px)");
        }
        Renderer2D::DrawLabel(headerLabel, x, uvHeaderY + 2.0f, 14, { 160, 200, 245, 255 });

        // Quick helper buttons on the right side of the UV header: [Full] [64x64] [<] [>]
        float btnH = 19.0f;
        float rightEdge = x + w;
        float stepBtnW = 22.0f;
        float cropBtnW = 44.0f;
        float fullBtnW = 38.0f;
        float gap = 3.0f;

        float nextBtnX = rightEdge - stepBtnW;
        float prevBtnX = nextBtnX - gap - stepBtnW;
        float cropBtnX = prevBtnX - gap - cropBtnW;
        float fullBtnX = cropBtnX - gap - fullBtnW;

        bool isFullUV = (sprite.sourceX == 0.0f && sprite.sourceY == 0.0f &&
                         sprite.sourceW <= 0.0f && sprite.sourceH <= 0.0f);
        if (DrawButton(fullBtnX, uvHeaderY, fullBtnW, btnH, "Full",
                       isFullUV ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                       { 64, 128, 210, 255 })) {
            PushUndoIfActive();
            sprite.sourceX = 0.0f;
            sprite.sourceY = 0.0f;
            sprite.sourceW = 0.0f;
            sprite.sourceH = 0.0f;
            sprite.frameCount = 1;
            sprite.currentFrame = 0;
            sprite.isPlaying = false;
            sprite.frameTimer = 0.0f;
            m_focusedTextFieldId = -1;
        }

        bool is64x64 = (std::fabs(sprite.sourceW - 64.0f) < 0.5f && std::fabs(sprite.sourceH - 64.0f) < 0.5f);
        if (DrawButton(cropBtnX, uvHeaderY, cropBtnW, btnH, "64x64",
                       is64x64 ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                       { 64, 128, 210, 255 })) {
            PushUndoIfActive();
            sprite.sourceW = 64.0f;
            sprite.sourceH = 64.0f;
            m_focusedTextFieldId = -1;
        }

        float stepW = (sprite.sourceW > 0.0f) ? sprite.sourceW : 64.0f;
        if (DrawButton(prevBtnX, uvHeaderY, stepBtnW, btnH, "<", { 40, 44, 56, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            if (sprite.frameCount > 1) {
                sprite.isPlaying = false;
                sprite.currentFrame = (sprite.currentFrame - 1 + sprite.frameCount) % sprite.frameCount;
                sprite.SyncFrameUV(texW, texH);
            } else {
                if (sprite.sourceW <= 0.0f) sprite.sourceW = 64.0f;
                if (sprite.sourceH <= 0.0f) sprite.sourceH = 64.0f;
                sprite.sourceX -= stepW;
                if (sprite.sourceX < 0.0f) {
                    sprite.sourceX = std::max(0.0f, maxW - stepW);
                }
            }
            m_focusedTextFieldId = -1;
        }

        if (DrawButton(nextBtnX, uvHeaderY, stepBtnW, btnH, ">", { 40, 44, 56, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            if (sprite.frameCount > 1) {
                sprite.isPlaying = false;
                sprite.currentFrame = (sprite.currentFrame + 1) % sprite.frameCount;
                sprite.SyncFrameUV(texW, texH);
            } else {
                if (sprite.sourceW <= 0.0f) sprite.sourceW = 64.0f;
                if (sprite.sourceH <= 0.0f) sprite.sourceH = 64.0f;
                sprite.sourceX += stepW;
                if (sprite.sourceX >= maxW) {
                    sprite.sourceX = 0.0f;
                }
            }
            m_focusedTextFieldId = -1;
        }

        // 3. UV Sub-Rectangle Sliders (Source X, Source Y, Source W, Source H)
        float sy = uvHeaderY + 22.0f;
        DrawFloatSlider(x, sy,         w, "Source X", sprite.sourceX, 0.0f, maxW, baseSliderId + 0);
        DrawFloatSlider(x, sy + 20.0f, w, "Source Y", sprite.sourceY, 0.0f, maxH, baseSliderId + 1);
        DrawFloatSlider(x, sy + 40.0f, w, "Source W", sprite.sourceW, 0.0f, maxW, baseSliderId + 2);
        DrawFloatSlider(x, sy + 60.0f, w, "Source H", sprite.sourceH, 0.0f, maxH, baseSliderId + 3);

        sprite.sourceX = std::round(sprite.sourceX);
        sprite.sourceY = std::round(sprite.sourceY);
        sprite.sourceW = std::round(sprite.sourceW);
        sprite.sourceH = std::round(sprite.sourceH);
    }

    void EditorGui::DrawSpriteAnimatorControls(float x, float y, float w, Scene::SpriteComponent& sprite, int baseSliderId) {
        int texW = 0, texH = 0;
        bool hasTex = Renderer2D::GetTextureSize(sprite.texturePath.c_str(), texW, texH);

        sprite.frameCount = std::max(1, sprite.frameCount);
        sprite.currentFrame = std::max(0, std::min(sprite.currentFrame, sprite.frameCount - 1));

        // 1. Section Sub-Header + Live Frame/FPS Badge
        Renderer2D::DrawLabel("2D Sprite Animator", x, y + 1.0f, 14, { 110, 225, 195, 255 });

        char badgeBuf[48];
        if (sprite.frameCount > 1) {
            std::snprintf(badgeBuf, sizeof(badgeBuf), "[F %d/%d | %.0f FPS]",
                          sprite.currentFrame + 1, sprite.frameCount, sprite.fps);
        } else {
            std::snprintf(badgeBuf, sizeof(badgeBuf), "[Static 1f]");
        }
        int badgeW = Renderer2D::MeasureTextWidth(badgeBuf, 14);
        Renderer2D::DrawLabel(badgeBuf, x + w - static_cast<float>(badgeW), y + 1.0f, 14,
                              sprite.isPlaying ? ColorRGBA{ 105, 235, 150, 255 } : ColorRGBA{ 155, 165, 185, 255 });

        // 2. Playback Row: [Playing] checkbox, [Loop] checkbox, and [|<] [<] [>] transport buttons
        float row2Y = y + 21.0f;
        bool wasPlaying = sprite.isPlaying;
        if (DrawCheckbox(x, row2Y, "Playing", sprite.isPlaying)) {
            if (!wasPlaying && sprite.isPlaying) {
                if (sprite.frameCount <= 1) {
                    if (hasTex && texH > 0 && texW > texH && (texW % texH == 0)) {
                        sprite.frameCount = texW / texH;
                    } else {
                        sprite.frameCount = 4;
                    }
                    if (hasTex && texW > 0) {
                        sprite.sourceW = std::round(static_cast<float>(texW) / static_cast<float>(sprite.frameCount));
                        if (sprite.sourceH <= 0.0f) sprite.sourceH = static_cast<float>(texH);
                    }
                }
                if (!sprite.loop && sprite.currentFrame >= sprite.frameCount - 1) {
                    sprite.currentFrame = 0;
                }
                sprite.frameTimer = 0.0f;
                sprite.SyncFrameUV(texW, texH);
            }
        }

        DrawCheckbox(x + 96.0f, row2Y, "Loop", sprite.loop);

        float btnH = 18.0f;
        float stepBtnW = 24.0f;
        float gap = 3.0f;
        float rightEdge = x + w;
        float nextFrameX = rightEdge - stepBtnW;
        float prevFrameX = nextFrameX - gap - stepBtnW;
        float firstFrameX = prevFrameX - gap - 26.0f;

        if (DrawButton(firstFrameX, row2Y, 26.0f, btnH, "|<", { 40, 44, 56, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            sprite.currentFrame = 0;
            sprite.frameTimer = 0.0f;
            sprite.SyncFrameUV(texW, texH);
            m_focusedTextFieldId = -1;
        }
        if (DrawButton(prevFrameX, row2Y, stepBtnW, btnH, "<", { 40, 44, 56, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            sprite.isPlaying = false;
            sprite.frameTimer = 0.0f;
            if (sprite.frameCount > 1) {
                sprite.currentFrame = (sprite.currentFrame - 1 + sprite.frameCount) % sprite.frameCount;
                sprite.SyncFrameUV(texW, texH);
            }
            m_focusedTextFieldId = -1;
        }
        if (DrawButton(nextFrameX, row2Y, stepBtnW, btnH, ">", { 40, 44, 56, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            sprite.isPlaying = false;
            sprite.frameTimer = 0.0f;
            if (sprite.frameCount > 1) {
                sprite.currentFrame = (sprite.currentFrame + 1) % sprite.frameCount;
                sprite.SyncFrameUV(texW, texH);
            }
            m_focusedTextFieldId = -1;
        }

        // 3. Frame Count, Current Frame Scrubber, and Playback Speed (FPS) Sliders
        float sy = row2Y + 23.0f;
        int prevFrameCount = sprite.frameCount;
        DrawIntSlider(x, sy, w, "Frames", sprite.frameCount, 1, 16, baseSliderId + 0);
        if (sprite.frameCount != prevFrameCount) {
            sprite.currentFrame = std::max(0, std::min(sprite.currentFrame, sprite.frameCount - 1));
            if (sprite.frameCount > 1) {
                if (hasTex && texW > 0) {
                    sprite.sourceW = std::round(static_cast<float>(texW) / static_cast<float>(sprite.frameCount));
                    if (sprite.sourceH <= 0.0f) sprite.sourceH = static_cast<float>(texH);
                } else if (sprite.sourceW <= 0.0f) {
                    sprite.sourceW = 64.0f;
                    sprite.sourceH = 64.0f;
                }
                sprite.SyncFrameUV(texW, texH);
            } else {
                sprite.currentFrame = 0;
                sprite.isPlaying = false;
            }
        }

        int prevFrame = sprite.currentFrame;
        int maxFrameIdx = std::max(1, sprite.frameCount - 1);
        DrawIntSlider(x, sy + 20.0f, w, "Frame #", sprite.currentFrame, 0, maxFrameIdx, baseSliderId + 1);
        sprite.currentFrame = std::max(0, std::min(sprite.currentFrame, sprite.frameCount - 1));
        if (sprite.currentFrame != prevFrame) {
            sprite.frameTimer = 0.0f;
            sprite.SyncFrameUV(texW, texH);
        }

        DrawFloatSlider(x, sy + 40.0f, w, "Speed FPS", sprite.fps, 1.0f, 30.0f, baseSliderId + 2);
        sprite.fps = std::round(sprite.fps);
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

    void EditorGui::DrawColorPickerRow(float x, float y, float w, const char* label,
                                       ColorRGBA& color, int baseSliderId) {
        Renderer2D::DrawLabel(label, x, y + 2.0f, 14, { 185, 190, 205, 255 });

        Renderer2D::DrawRect(x + 84.0f, y, 22.0f, 19.0f, color);
        Renderer2D::DrawRectOutline(x + 84.0f, y, 22.0f, 19.0f, 1.0f, { 220, 220, 230, 255 });

        float swatchX = x + 114.0f;
        float swatchSize = 22.0f;
        for (int i = 0; i < NUM_PRESET_COLORS; ++i) {
            Renderer2D::DrawRect(swatchX, y, swatchSize, 19.0f, PRESET_COLORS[i]);
            bool hovered = IsMouseOver(swatchX, y, swatchSize, 19.0f);
            if (hovered) {
                Renderer2D::DrawRectOutline(swatchX, y, swatchSize, 19.0f, 2.0f, { 255, 255, 255, 255 });
                if (Input::IsMouseButtonPressed(MouseButton::Left)) {
                    if (!EditorUndoRedo::AreColorsEqual(color, PRESET_COLORS[i])) {
                        PushUndoIfActive();
                    }
                    color = PRESET_COLORS[i];
                }
            }
            swatchX += swatchSize + 4.0f;
        }

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

    void EditorGui::DrawTextInputBox(float x, float y, float w, const char* label,
                                     std::string& textBuffer, int fieldId) {
        Renderer2D::DrawLabel(label, x, y + 4.0f, 14, { 185, 190, 205, 255 });

        float boxX = x + 54.0f;
        float boxW = w - 54.0f;
        float boxH = 23.0f;
        float textStartX = boxX + 8.0f;
        constexpr int fontSize = 14;

        bool hovered = IsMouseOver(boxX, y, boxW, boxH);

        if (hovered && Input::IsMouseButtonPressed(MouseButton::Left)) {
            bool wasAlreadyFocused = (m_focusedTextFieldId == fieldId);
            m_focusedTextFieldId = fieldId;

            int clickedIndex = GetTextCharIndexAtMouseX(textBuffer, textStartX, Input::GetMouseX(), fontSize);
            m_textCursorPos = clickedIndex;

            bool shiftDown = Input::IsKeyDown(Key::LeftShift) || Input::IsKeyDown(Key::RightShift);
            if (!wasAlreadyFocused || !shiftDown) {
                m_textSelectionAnchor = clickedIndex;
            }
            m_isDraggingText = true;
        }

        bool isFocused = (m_focusedTextFieldId == fieldId);

        if (isFocused && m_isDraggingText && Input::IsMouseButtonDown(MouseButton::Left)) {
            m_textCursorPos = GetTextCharIndexAtMouseX(textBuffer, textStartX, Input::GetMouseX(), fontSize);
        }

        Renderer2D::DrawRect(boxX, y, boxW, boxH, isFocused ? ColorRGBA{ 45, 50, 66, 255 } : ColorRGBA{ 34, 36, 46, 255 });
        Renderer2D::DrawRectOutline(boxX, y, boxW, boxH, isFocused ? 2.0f : 1.0f,
                                    isFocused ? ColorRGBA{ 95, 195, 255, 255 } : ColorRGBA{ 75, 80, 98, 255 });

        if (!isFocused && textBuffer.empty() && fieldId == 3) {
            Renderer2D::DrawLabel("(none - solid color box)", textStartX, y + 4.0f, fontSize, { 110, 115, 130, 255 });
            return;
        }
        if (!isFocused && textBuffer.empty() && fieldId == 4) {
            Renderer2D::DrawLabel("(type new action name, e.g. shoot, heal, special_attack...)", textStartX, y + 4.0f, fontSize, { 110, 115, 130, 255 });
            return;
        }
        if (!isFocused && textBuffer.empty() && fieldId == 5) {
            Renderer2D::DrawLabel("(Self - follows this entity)", textStartX, y + 4.0f, fontSize, { 110, 115, 130, 255 });
            return;
        }

        if (isFocused) {
            int len = static_cast<int>(textBuffer.size());
            int cur = std::max(0, std::min(m_textCursorPos, len));
            int anc = std::max(0, std::min(m_textSelectionAnchor, len));

            if (cur != anc) {
                int selStart = std::min(cur, anc);
                int selEnd   = std::max(cur, anc);

                std::string prefixStart = textBuffer.substr(0, static_cast<size_t>(selStart));
                std::string prefixEnd   = textBuffer.substr(0, static_cast<size_t>(selEnd));

                float selX1 = textStartX + static_cast<float>(Renderer2D::MeasureTextWidth(prefixStart.c_str(), fontSize));
                float selX2 = textStartX + static_cast<float>(Renderer2D::MeasureTextWidth(prefixEnd.c_str(), fontSize));

                Renderer2D::DrawRect(selX1, y + 3.0f, selX2 - selX1, boxH - 6.0f, { 55, 125, 215, 185 });
            }
        }

        Renderer2D::DrawLabel(textBuffer.c_str(), textStartX, y + 4.0f, fontSize, { 245, 245, 250, 255 });

        if (isFocused) {
            int len = static_cast<int>(textBuffer.size());
            int cur = std::max(0, std::min(m_textCursorPos, len));
            std::string prefixCursor = textBuffer.substr(0, static_cast<size_t>(cur));
            float cursorX = textStartX + static_cast<float>(Renderer2D::MeasureTextWidth(prefixCursor.c_str(), fontSize));

            Renderer2D::DrawRect(cursorX, y + 4.0f, 2.0f, boxH - 8.0f, { 110, 210, 255, 255 });
        }
    }

}

