// EditorGui: text field editing (caret, selection, word jump) and the text input box
#include "editor/gui/editor_gui.h"
#include "core/input.h"
#include <algorithm>
#include <string>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Scene;

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
            if (m_focusedTextFieldId == 6) { targetStr = &selected->tag; isEntityField = true; }
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
