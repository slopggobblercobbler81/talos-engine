#include "editor/dialogs/input_map_dialog.h"
#include "core/input.h"
#include "renderer/renderer2d.h"
#include <algorithm>
#include <cstdio>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Editor::EditorLayout;

    void InputMapDialog::Initialize(const std::string& filePath) {
        m_filePath = filePath;
        if (!InputMap::Load(m_filePath)) {
            InputMap::Save(m_filePath);
        }
    }

    void InputMapDialog::Toggle(EditorGui& gui) {
        m_isOpen = !m_isOpen;
        m_listeningActionName.clear();
        if (!m_isOpen && gui.GetFocusedTextFieldId() == 4) {
            gui.UnfocusTextField();
        }
    }

    void InputMapDialog::Close(EditorGui& gui) {
        m_isOpen = false;
        m_listeningActionName.clear();
        if (gui.GetFocusedTextFieldId() == 4) {
            gui.UnfocusTextField();
        }
    }

    bool InputMapDialog::UpdateKeyListening(std::string& outStatusMessage) {
        if (!m_isOpen || m_listeningActionName.empty()) {
            return false;
        }

        if (Input::IsMouseButtonPressed(MouseButton::Right)) {
            m_listeningActionName.clear();
            outStatusMessage = "Key binding cancelled";
            return true;
        }

        Key pressedKey = Key::Unknown;
        if (Input::PollAnyKeyPressed(pressedKey)) {
            while (Input::GetCharPressed() > 0) {}
            std::string targetAction = m_listeningActionName;
            m_listeningActionName.clear();
            if (InputMap::BindKey(targetAction, pressedKey)) {
                InputMap::Save(m_filePath);
                outStatusMessage = "Bound [" + std::string(Input::GetKeyName(pressedKey)) +
                                   "] -> '" + targetAction + "'";
            } else {
                outStatusMessage = "Key [" + std::string(Input::GetKeyName(pressedKey)) +
                                   "] already bound to '" + targetAction + "'";
            }
            return true;
        }

        return false;
    }

    void InputMapDialog::SubmitNewAction(EditorGui& gui, std::string& outStatusMessage) {
        if (m_newActionNameBuffer.empty()) {
            gui.SetFocusedTextFieldId(4);
            outStatusMessage = "Type an action name first!";
            return;
        }

        std::string actionName = m_newActionNameBuffer;
        if (InputMap::AddAction(actionName)) {
            InputMap::Save(m_filePath);
            outStatusMessage = "Created action '" + actionName + "'";
            m_newActionNameBuffer.clear();
            gui.ResetTextCaret();
            m_scrollOffset = 9999; // Scroll to bottom so new action is immediately visible
        } else {
            outStatusMessage = "Action '" + actionName + "' already exists!";
        }
    }

    void InputMapDialog::Render(int windowWidth,
                                int windowHeight,
                                float consoleHeight,
                                EditorGui& gui,
                                std::string& outStatusMessage) {
        float vpX = LEFT_PANEL_WIDTH;
        float vpY = TOOLBAR_HEIGHT;
        float vpW = static_cast<float>(windowWidth) - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
        float vpH = static_cast<float>(windowHeight) - TOOLBAR_HEIGHT - consoleHeight;

        // 1. Translucent dark backdrop over the central viewport
        Renderer2D::DrawRect(vpX, vpY, vpW, vpH, { 10, 11, 16, 200 });

        // Centered modal window dimensions
        float modalW = std::min(720.0f, static_cast<float>(windowWidth) - 80.0f);
        float modalH = std::min(490.0f, static_cast<float>(windowHeight) - TOOLBAR_HEIGHT - 40.0f);
        float modalX = (static_cast<float>(windowWidth) - modalW) * 0.5f;
        float modalY = TOOLBAR_HEIGHT + std::max(14.0f, (static_cast<float>(windowHeight) - TOOLBAR_HEIGHT - modalH) * 0.42f);

        // Modal shadow, background card, and accent border
        Renderer2D::DrawRect(modalX + 4.0f, modalY + 4.0f, modalW, modalH, { 5, 6, 10, 160 });
        Renderer2D::DrawRect(modalX, modalY, modalW, modalH, { 22, 24, 31, 255 });
        Renderer2D::DrawRectOutline(modalX, modalY, modalW, modalH, 2.0f, { 75, 140, 215, 255 });

        // 2. Modal Header Bar
        constexpr float headerH = 34.0f;
        Renderer2D::DrawRect(modalX + 2.0f, modalY + 2.0f, modalW - 4.0f, headerH, { 28, 32, 42, 255 });
        Renderer2D::DrawLine2D(modalX, modalY + headerH + 2.0f, modalX + modalW, modalY + headerH + 2.0f, { 55, 65, 85, 255 });

        Renderer2D::DrawLabel(
            "INPUT MAP  -  PROJECT ACTIONS & KEY BINDINGS",
            modalX + 14.0f, modalY + 10.0f, 15, { 95, 200, 255, 255 }
        );

        // Header Right Buttons: Reset Defaults & Close [X]
        if (gui.DrawButton(modalX + modalW - 208.0f, modalY + 6.0f, 116.0f, 24.0f, "Reset Defaults",
                           { 55, 60, 78, 255 }, { 75, 82, 106, 255 })) {
            InputMap::InitDefaults();
            InputMap::Save(m_filePath);
            m_listeningActionName.clear();
            m_scrollOffset = 0;
            outStatusMessage = "Reset Input Map to default video game actions";
        }

        if (gui.DrawButton(modalX + modalW - 84.0f, modalY + 6.0f, 72.0f, 24.0f, "Close [X]",
                           { 140, 48, 54, 255 }, { 175, 64, 70, 255 })) {
            Close(gui);
            return;
        }

        // 3. Add New Action Bar (Godot-style top input row)
        float addRowY = modalY + headerH + 10.0f;
        gui.DrawTextInputBox(modalX + 14.0f, addRowY, modalW - 154.0f, "Action:", m_newActionNameBuffer, 4);

        if (gui.DrawButton(modalX + modalW - 130.0f, addRowY - 1.0f, 116.0f, 25.0f, "+ Add Action",
                           { 42, 140, 82, 255 }, { 56, 170, 102, 255 })) {
            SubmitNewAction(gui, outStatusMessage);
        }

        // 4. Column Header Row
        float colHdrY = addRowY + 32.0f;
        Renderer2D::DrawRect(modalX + 2.0f, colHdrY, modalW - 4.0f, 22.0f, { 18, 20, 26, 255 });
        Renderer2D::DrawLine2D(modalX + 2.0f, colHdrY + 22.0f, modalX + modalW - 2.0f, colHdrY + 22.0f, { 45, 50, 64, 255 });

        float colStateX  = modalX + 14.0f;
        float colNameX   = modalX + 70.0f;
        float colKeysX   = modalX + 230.0f;
        float colManageX = modalX + modalW - 156.0f;

        ColorRGBA hdrColor = { 140, 148, 168, 255 };
        Renderer2D::DrawLabel("STATE",                            colStateX,  colHdrY + 4.0f, 14, hdrColor);
        Renderer2D::DrawLabel("ACTION NAME (CODE ID)",            colNameX,   colHdrY + 4.0f, 14, hdrColor);
        Renderer2D::DrawLabel("BOUND KEYS (CLICK KEY TO UNBIND)", colKeysX,   colHdrY + 4.0f, 14, hdrColor);
        Renderer2D::DrawLabel("ASSIGN / DELETE",                  colManageX, colHdrY + 4.0f, 14, hdrColor);

        // 5. Scrollable Action List Body
        constexpr float footerH = 30.0f;
        float listY = colHdrY + 23.0f;
        float listH = (modalY + modalH - footerH) - listY;
        constexpr float rowH = 30.0f;

        const auto& actions = InputMap::GetActions();
        int totalRows = static_cast<int>(actions.size());
        int maxVisibleRows = std::max(1, static_cast<int>((listH - 6.0f) / rowH));
        int maxScroll = std::max(0, totalRows - maxVisibleRows);

        if (EditorGui::IsMouseOver(modalX, listY, modalW, listH)) {
            float wheel = Input::GetMouseWheelMove();
            if (wheel > 0.01f) {
                m_scrollOffset = std::max(0, m_scrollOffset - 1);
            } else if (wheel < -0.01f) {
                m_scrollOffset = std::min(maxScroll, m_scrollOffset + 1);
            }
        }
        m_scrollOffset = std::max(0, std::min(m_scrollOffset, maxScroll));

        std::string actionToDelete;
        std::string actionToUnbind;
        Key keyToUnbind = Key::Unknown;

        Renderer2D::BeginViewportScissor(
            static_cast<int>(modalX + 2.0f),
            static_cast<int>(listY),
            static_cast<int>(modalW - 4.0f),
            static_cast<int>(listH)
        );

        int startIdx = m_scrollOffset;
        int endIdx   = std::min(totalRows, startIdx + maxVisibleRows);
        float rowY   = listY + 3.0f;

        for (int i = startIdx; i < endIdx; ++i) {
            const InputAction& action = actions[static_cast<size_t>(i)];
            bool isListeningThis = (m_listeningActionName == action.name);
            bool isHeldNow       = InputMap::IsActionDown(action.name);

            ColorRGBA rowBg = (i % 2 == 0) ? ColorRGBA{ 26, 28, 36, 255 } : ColorRGBA{ 22, 24, 31, 255 };
            if (isListeningThis) {
                rowBg = { 52, 42, 22, 255 };
            } else if (isHeldNow) {
                rowBg = { 24, 44, 34, 255 };
            }
            Renderer2D::DrawRect(modalX + 6.0f, rowY, modalW - 18.0f, rowH - 2.0f, rowBg);

            // Live State Badge ([ON] when any bound key is held, [OFF] otherwise)
            ColorRGBA stateBg = isHeldNow ? ColorRGBA{ 42, 175, 95, 255 } : ColorRGBA{ 38, 41, 52, 255 };
            Renderer2D::DrawRect(colStateX, rowY + 4.0f, 44.0f, 20.0f, stateBg);
            Renderer2D::DrawRectOutline(colStateX, rowY + 4.0f, 44.0f, 20.0f, 1.0f,
                                        isHeldNow ? ColorRGBA{ 95, 235, 145, 255 } : ColorRGBA{ 62, 68, 84, 255 });
            Renderer2D::DrawLabel(isHeldNow ? "ON" : "OFF",
                                  colStateX + (isHeldNow ? 13.0f : 9.0f), rowY + 7.0f, 14,
                                  isHeldNow ? ColorRGBA{ 255, 255, 255, 255 } : ColorRGBA{ 130, 136, 152, 255 });

            // Action Identifier Name
            Renderer2D::DrawLabel(action.name.c_str(), colNameX, rowY + 7.0f, 15,
                                  isHeldNow ? ColorRGBA{ 115, 245, 160, 255 } : ColorRGBA{ 235, 238, 248, 255 });

            // Bound Physical Keys (clickable chips to unbind)
            float keyCursorX = colKeysX;
            float maxKeysRightX = colManageX - 10.0f;

            if (action.keys.empty()) {
                Renderer2D::DrawLabel("(no keys assigned)", keyCursorX, rowY + 7.0f, 14, { 115, 120, 136, 255 });
            } else {
                for (Key k : action.keys) {
                    std::string chipLabel = std::string(Input::GetKeyName(k)) + "  x";
                    int chipTextW = Renderer2D::MeasureTextWidth(chipLabel.c_str(), 14);
                    float chipW = static_cast<float>(chipTextW) + 16.0f;

                    if (keyCursorX + chipW > maxKeysRightX) {
                        Renderer2D::DrawLabel("...", keyCursorX, rowY + 7.0f, 14, { 160, 165, 180, 255 });
                        break;
                    }

                    bool keyHeld = Input::IsKeyDown(k);
                    ColorRGBA chipBg = keyHeld ? ColorRGBA{ 45, 145, 85, 255 } : ColorRGBA{ 44, 52, 70, 255 };
                    ColorRGBA chipHover = { 155, 52, 58, 255 };

                    if (gui.DrawButton(keyCursorX, rowY + 3.0f, chipW, 22.0f, chipLabel.c_str(), chipBg, chipHover)) {
                        actionToUnbind = action.name;
                        keyToUnbind = k;
                    }
                    keyCursorX += chipW + 6.0f;
                }
            }

            // Right-side Manage Buttons: "+ Bind Key" (or "Press Key...") and "Del"
            if (isListeningThis) {
                if (gui.DrawButton(colManageX, rowY + 3.0f, 92.0f, 22.0f, "Press Key...",
                                   { 205, 135, 32, 255 }, { 225, 155, 48, 255 })) {
                    m_listeningActionName.clear();
                }
            } else {
                if (gui.DrawButton(colManageX, rowY + 3.0f, 92.0f, 22.0f, "+ Bind Key",
                                   { 48, 105, 185, 255 }, { 65, 130, 220, 255 })) {
                    m_listeningActionName = action.name;
                    gui.UnfocusTextField();
                }
            }

            if (gui.DrawButton(colManageX + 98.0f, rowY + 3.0f, 44.0f, 22.0f, "Del",
                               { 125, 45, 50, 255 }, { 165, 60, 66, 255 })) {
                actionToDelete = action.name;
            }

            rowY += rowH;
        }

        if (totalRows > maxVisibleRows) {
            float trackX = modalX + modalW - 9.0f;
            float trackY = listY + 3.0f;
            float trackH = listH - 6.0f;
            Renderer2D::DrawRect(trackX, trackY, 4.0f, trackH, { 32, 35, 44, 255 });

            float thumbRatio = static_cast<float>(maxVisibleRows) / static_cast<float>(totalRows);
            float thumbH = std::max(18.0f, trackH * thumbRatio);
            float scrollFraction = static_cast<float>(m_scrollOffset) / static_cast<float>(maxScroll);
            float thumbY = trackY + (trackH - thumbH) * scrollFraction;
            Renderer2D::DrawRect(trackX, thumbY, 4.0f, thumbH, { 95, 145, 215, 255 });
        }

        Renderer2D::EndViewportScissor();

        // Execute deferred unbind / delete actions after iterating
        if (!actionToUnbind.empty() && keyToUnbind != Key::Unknown) {
            if (InputMap::UnbindKey(actionToUnbind, keyToUnbind)) {
                InputMap::Save(m_filePath);
                outStatusMessage = "Unbound [" + std::string(Input::GetKeyName(keyToUnbind)) +
                                   "] from '" + actionToUnbind + "'";
            }
        }
        if (!actionToDelete.empty()) {
            if (m_listeningActionName == actionToDelete) {
                m_listeningActionName.clear();
            }
            if (InputMap::RemoveAction(actionToDelete)) {
                InputMap::Save(m_filePath);
                outStatusMessage = "Removed action '" + actionToDelete + "'";
            }
        }

        // 6. Modal Footer Bar
        float footerY = modalY + modalH - footerH;
        Renderer2D::DrawRect(modalX + 2.0f, footerY, modalW - 4.0f, footerH - 2.0f, { 18, 20, 26, 255 });
        Renderer2D::DrawLine2D(modalX + 2.0f, footerY, modalX + modalW - 2.0f, footerY, { 45, 50, 64, 255 });

        if (!m_listeningActionName.empty()) {
            std::string prompt = "LISTENING FOR KEY -> Press any keyboard key to bind to '" +
                                 m_listeningActionName + "' (Right-Click to cancel)";
            Renderer2D::DrawLabel(prompt.c_str(), modalX + 14.0f, footerY + 7.0f, 14, { 255, 210, 80, 255 });
        } else {
            char footerBuf[180];
            std::snprintf(
                footerBuf, sizeof(footerBuf),
                "Actions: %zu  |  Saved to: %s  |  Code API: InputMap::IsActionDown(\"jump\")",
                actions.size(), m_filePath.c_str()
            );
            Renderer2D::DrawLabel(footerBuf, modalX + 14.0f, footerY + 7.0f, 14, { 145, 155, 178, 255 });
        }
    }

}
