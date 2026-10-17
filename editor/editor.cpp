#include "editor/editor.h"
#include "scene/scene_serializer.h"
#include "core/input.h"
#include "core/logger.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

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

    // Built-in 1-click texture presets pointing to real .png files in "examples/assets/textures/"
    struct TexturePreset {
        const char* label;
        const char* path;
    };

    static const TexturePreset TEXTURE_PRESETS[] = {
        { "None",  "" },
        { "Hero",  "examples/assets/textures/hero.png" },
        { "Crate", "examples/assets/textures/crate.png" },
        { "Brick", "examples/assets/textures/brick.png" },
        { "Coin",  "examples/assets/textures/coin.png" }
    };
    static constexpr int NUM_TEXTURE_PRESETS = 5;

    // Helper to test if the mouse cursor lies inside a 2D UI rectangle
    static bool IsMouseOver(float x, float y, float w, float h) {
        float mx = Input::GetMouseX();
        float my = Input::GetMouseY();
        return (mx >= x && mx <= x + w && my >= y && my <= y + h);
    }

    // Helper: Returns true if the character belongs to a word (alphanumeric or underscore).
    // Used by Ctrl+Left / Ctrl+Right word-by-word navigation.
    static bool IsWordChar(char c) {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') ||
               (c == '_');
    }
    
    // Helper: Finds the next word boundary index when jumping with Ctrl + Left/Right Arrow.
    // dir < 0 jumps to the start of the previous word; dir > 0 jumps to the end/next word.
    static int FindNextWordBoundary(const std::string& text, int currentPos, int dir) {
        int len = static_cast<int>(text.size());
        int pos = std::max(0, std::min(currentPos, len));
        if (dir < 0) {
            // Move left: first skip any non-word separators (spaces, slashes, etc.)
            while (pos > 0 && !IsWordChar(text[pos - 1])) {
                --pos;
            }
            // Then skip all characters belonging to the word until its beginning
            while (pos > 0 && IsWordChar(text[pos - 1])) {
                --pos;
            }
        } else if (dir > 0) {
            // Move right: first skip any non-word separators
            while (pos < len && !IsWordChar(text[pos])) {
                ++pos;
            }
            // Then skip all characters belonging to the current word
            while (pos < len && IsWordChar(text[pos])) {
                ++pos;
            }
        }
        return pos;
    }

    // Helper: Converts a screen-space mouse X coordinate into the closest character index
    // (0 .. text.size()) inside a text input box using TrueType glyph width measurements.
    static int GetTextCharIndexAtMouseX(const std::string& text, float textStartX, float mouseX, int fontSize) {
        float relX = mouseX - textStartX;
        if (relX <= 0.0f || text.empty()) {
            return 0;
        }
        int len = static_cast<int>(text.size());
        int prevWidth = 0;
        for (int i = 1; i <= len; ++i) {
            std::string prefix = text.substr(0, static_cast<size_t>(i));
            int curWidth = Renderer2D::MeasureTextWidth(prefix.c_str(), fontSize);
            // Check if the mouse lies before the horizontal midpoint of character (i - 1)
            float charMidpoint = (static_cast<float>(prevWidth) + static_cast<float>(curWidth)) * 0.5f;
            if (relX < charMidpoint) {
                return i - 1;
            }
            prevWidth = curWidth;
        }
        return len;
    }


    // Internal comparison helper: Returns true if two ColorRGBA values are identical.
    static bool AreColorsEqual(const ColorRGBA& a, const ColorRGBA& b) {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    }

    // Internal comparison helper: Returns true if two Entity instances have identical state.
    // Used by CommitContinuousEdit() to verify that a viewport drag or slider drag actually
    // modified the scene before pushing a new entry onto the Undo stack.
    static bool AreEntityListsEqual(const std::vector<Entity>& a, const std::vector<Entity>& b) {
        if (a.size() != b.size()) return false;

        for (size_t i = 0; i < a.size(); ++i) {
            const Entity& ea = a[i];
            const Entity& eb = b[i];
            if (ea.id != eb.id || ea.name != eb.name || ea.active != eb.active) return false;
            if (std::fabs(ea.transform.x - eb.transform.x) > 0.01f ||
                std::fabs(ea.transform.y - eb.transform.y) > 0.01f ||
                std::fabs(ea.transform.width - eb.transform.width) > 0.01f ||
                std::fabs(ea.transform.height - eb.transform.height) > 0.01f) return false;
            if (ea.sprite.enabled != eb.sprite.enabled ||
                ea.sprite.texturePath != eb.sprite.texturePath ||
                !AreColorsEqual(ea.sprite.color, eb.sprite.color)) return false;
            if (ea.text.enabled != eb.text.enabled ||
                ea.text.content != eb.text.content ||
                ea.text.fontSize != eb.text.fontSize ||
                !AreColorsEqual(ea.text.color, eb.text.color) ||
                std::fabs(ea.text.offsetX - eb.text.offsetX) > 0.01f ||
                std::fabs(ea.text.offsetY - eb.text.offsetY) > 0.01f) return false;
            if (ea.collider.enabled != eb.collider.enabled ||
                ea.collider.isSolid != eb.collider.isSolid ||
                ea.collider.isStatic != eb.collider.isStatic) return false;
            if (ea.movement.enabled != eb.movement.enabled ||
                ea.movement.playerControlled != eb.movement.playerControlled ||
                std::fabs(ea.movement.speed - eb.movement.speed) > 0.01f) return false;
        }
        return true;
    }

    Editor::Editor(int windowWidth, int windowHeight, const std::string& sceneFilePath)
        : m_windowWidth(windowWidth), m_windowHeight(windowHeight), m_sceneFilePath(sceneFilePath) {
        if (!InputMap::Load(m_inputMapFilePath)) {
            InputMap::Save(m_inputMapFilePath);
        }
        TALOS_LOG_INFO("Visual Editor initialized (scene target: '" + m_sceneFilePath + "')");
    }

    // =========================================================================
    // Universal Undo / Redo History Implementation
    // =========================================================================

    void Editor::PushUndoState(const Scene::Scene& scene) {
        if (m_isPlaying) return;

        // Avoid pushing duplicate consecutive states if nothing changed since the top of the stack
        if (!m_undoStack.empty() && AreEntityListsEqual(m_undoStack.back().entities, scene.GetEntities())) {
            return;
        }

        if (m_undoStack.size() >= MAX_UNDO_STEPS) {
            m_undoStack.erase(m_undoStack.begin());
        }

        m_undoStack.push_back({ scene.GetEntities(), m_selectedEntityId, m_textCursorPos, m_textSelectionAnchor });
        m_redoStack.clear();
    }

    void Editor::BeginContinuousEdit(const Scene::Scene& scene) {
        if (m_isPlaying || m_hasPendingSnapshot) return;
        m_pendingSnapshot = { scene.GetEntities(), m_selectedEntityId, m_textCursorPos, m_textSelectionAnchor };
        m_hasPendingSnapshot = true;
    }

    void Editor::CommitContinuousEdit(const Scene::Scene& scene) {
        if (!m_hasPendingSnapshot) return;
        m_hasPendingSnapshot = false;

        // Only push onto the Undo stack if the user's drag actually changed an entity property
        if (!AreEntityListsEqual(m_pendingSnapshot.entities, scene.GetEntities())) {
            if (m_undoStack.size() >= MAX_UNDO_STEPS) {
                m_undoStack.erase(m_undoStack.begin());
            }
            m_undoStack.push_back(m_pendingSnapshot);
            m_redoStack.clear();
        }
    }

    void Editor::Undo(Scene::Scene& scene) {
        if (m_isPlaying) return;

        // If the user triggers Ctrl+Z while still holding the mouse button mid-drag,
        // commit the pending drag start snapshot first and cancel the active drag.
        if (m_hasPendingSnapshot) {
            CommitContinuousEdit(scene);
            m_isDraggingEntity = false;
            m_activeSliderId = -1;
        }

        if (m_undoStack.empty()) {
            ShowStatusMessage("Nothing to undo");
            TALOS_LOG_WARN("Undo requested (Ctrl+Z), but the Undo history stack is empty");
            return;
        }

        // Save current state to Redo stack before restoring the previous Undo snapshot
        m_redoStack.push_back({ scene.GetEntities(), m_selectedEntityId, m_textCursorPos, m_textSelectionAnchor });

        EditorHistoryState restored = m_undoStack.back();
        m_undoStack.pop_back();

        scene.GetEntities() = restored.entities;
        m_selectedEntityId = restored.selectedEntityId;
        m_textCursorPos = restored.textCursorPos;
        m_textSelectionAnchor = restored.textSelectionAnchor;
        ShowStatusMessage("Undo (Ctrl+Z)");
        TALOS_LOG_INFO("Undo applied (Ctrl+Z) — restored previous scene snapshot");
    }

    void Editor::Redo(Scene::Scene& scene) {
        if (m_isPlaying) return;

        if (m_redoStack.empty()) {
            ShowStatusMessage("Nothing to redo");
            TALOS_LOG_WARN("Redo requested (Ctrl+Y), but the Redo history stack is empty");
            return;
        }

        // Save current state back onto the Undo stack before applying Redo
        m_undoStack.push_back({ scene.GetEntities(), m_selectedEntityId, m_textCursorPos, m_textSelectionAnchor });

        EditorHistoryState restored = m_redoStack.back();
        m_redoStack.pop_back();

        scene.GetEntities() = restored.entities;
        m_selectedEntityId = restored.selectedEntityId;
        m_textCursorPos = restored.textCursorPos;
        m_textSelectionAnchor = restored.textSelectionAnchor;
        ShowStatusMessage("Redo (Ctrl+Y)");
        TALOS_LOG_INFO("Redo applied (Ctrl+Y) — re-applied scene snapshot");
    }

    void Editor::StartPlayMode(const Scene::Scene& scene) {
        if (m_isPlaying) return;

        // Commit any in-progress drag before entering Play Mode
        CommitContinuousEdit(scene);

        // Capture a complete copy of all entities in memory before starting simulation.
        // Because Entities are flat POD-like structs, copying the vector takes microseconds.
        m_sceneSnapshot = scene.GetEntities();
        m_isPlaying = true;
        m_isDraggingEntity = false;
        m_focusedTextFieldId = -1;
        m_inputMapOpen = false;
        m_listeningActionName.clear();
        TALOS_LOG_INFO("Entered Play Mode (captured snapshot of " +
                       std::to_string(m_sceneSnapshot.size()) + " entities)");
    }

    void Editor::StopPlayMode(Scene::Scene& scene) {
        if (!m_isPlaying) return;

        // Instantly restore the pre-Play entity state (zero reload time).
        scene.GetEntities() = m_sceneSnapshot;
        m_isPlaying = false;
        TALOS_LOG_INFO("Stopped Play Mode (restored pre-Play scene snapshot)");
    }

    void Editor::ShowStatusMessage(const std::string& message, float durationSeconds) {
        m_statusMessage = message;
        m_statusTimer = durationSeconds;
    }

    void Editor::Update(Scene::Scene& scene, float delta) {
        m_activeScene = &scene;

        // Count down active status banner timer
        if (m_statusTimer > 0.0f) {
            m_statusTimer -= delta;
            if (m_statusTimer < 0.0f) {
                m_statusTimer = 0.0f;
            }
        }

        // Release active slider, viewport entity drag, or camera pan when mouse button is released
        if (Input::IsMouseButtonReleased(MouseButton::Left)) {
            CommitContinuousEdit(scene);
            m_activeSliderId = -1;
            m_isDraggingEntity = false;
            m_isDraggingText = false;
            if (!Input::IsMouseButtonDown(MouseButton::Middle) && !Input::IsMouseButtonDown(MouseButton::Right)) {
                m_isPanningCamera = false;
            }
        }
        if (Input::IsMouseButtonReleased(MouseButton::Middle) || Input::IsMouseButtonReleased(MouseButton::Right)) {
            if (!Input::IsMouseButtonDown(MouseButton::Middle) && !Input::IsMouseButtonDown(MouseButton::Right)) {
                m_isPanningCamera = false;
            }
        }

        // =====================================================================
        // Interactive Key Listening Mode (Input Map Key Assignment)
        // =====================================================================
        if (!m_isPlaying && m_inputMapOpen && !m_listeningActionName.empty()) {
            if (Input::IsMouseButtonPressed(MouseButton::Right)) {
                m_listeningActionName.clear();
                ShowStatusMessage("Key binding cancelled");
                return;
            }

            Key pressedKey = Key::Unknown;
            if (Input::PollAnyKeyPressed(pressedKey)) {
                while (Input::GetCharPressed() > 0) {}
                std::string targetAction = m_listeningActionName;
                m_listeningActionName.clear();
                if (InputMap::BindKey(targetAction, pressedKey)) {
                    InputMap::Save(m_inputMapFilePath);
                    ShowStatusMessage("Bound [" + std::string(Input::GetKeyName(pressedKey)) +
                                      "] -> '" + targetAction + "'");
                } else {
                    ShowStatusMessage("Key [" + std::string(Input::GetKeyName(pressedKey)) +
                                      "] already bound to '" + targetAction + "'");
                }
                return;
            }
        }

        // =====================================================================
        // Global Undo / Redo Keyboard Shortcuts (Ctrl + Z / Ctrl + Y)
        // =====================================================================
        // Processed before text field typing so Ctrl+Z works universally even
        // while a text input box (Name, Text, or Image path) is actively focused.
        bool ctrlDown = Input::IsKeyDown(Key::LeftControl) || Input::IsKeyDown(Key::RightControl);
        bool shiftDown = Input::IsKeyDown(Key::LeftShift) || Input::IsKeyDown(Key::RightShift);

        if (!m_isPlaying && ctrlDown) {
            // Drain character queue while Ctrl is held so shortcut keys never type into text boxes
            while (Input::GetCharPressed() > 0) {}

            if (Input::IsKeyPressed(Key::Z)) {
                if (shiftDown) {
                    Redo(scene);
                } else {
                    Undo(scene);
                }
                return;
            }
            if (Input::IsKeyPressed(Key::Y)) {
                Redo(scene);
                return;
            }
        }

        // Handle keyboard typing, cursor navigation, and text selection when a text box is focused
        if (m_focusedTextFieldId != -1 && !m_isPlaying) {
            Entity* selected = scene.FindEntityById(m_selectedEntityId);
            std::string* targetStr = nullptr;
            bool isEntityField = false;

            if (m_focusedTextFieldId == 4) {
                targetStr = &m_newActionNameBuffer;
                isEntityField = false;
            } else if (selected) {
                if (m_focusedTextFieldId == 1) { targetStr = &selected->name; isEntityField = true; }
                if (m_focusedTextFieldId == 2) { targetStr = &selected->text.content; isEntityField = true; }
                if (m_focusedTextFieldId == 3) { targetStr = &selected->sprite.texturePath; isEntityField = true; }
            }

            if (targetStr) {
                int len = static_cast<int>(targetStr->size());
                m_textCursorPos = std::max(0, std::min(m_textCursorPos, len));
                m_textSelectionAnchor = std::max(0, std::min(m_textSelectionAnchor, len));

                auto hasSelection = [&]() { return m_textCursorPos != m_textSelectionAnchor; };
                auto getSelStart  = [&]() { return std::min(m_textCursorPos, m_textSelectionAnchor); };
                auto getSelEnd    = [&]() { return std::max(m_textCursorPos, m_textSelectionAnchor); };

                // Helper lambda that erases the currently highlighted text range
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
                        // Pressing Left/Right without Shift collapses an active selection to its left/right boundary
                        m_textCursorPos = (dir < 0) ? getSelStart() : getSelEnd();
                        m_textSelectionAnchor = m_textCursorPos;
                    } else {
                        if (ctrlDown) {
                            m_textCursorPos = FindNextWordBoundary(*targetStr, m_textCursorPos, dir);
                        } else {
                            m_textCursorPos = std::max(0, std::min(m_textCursorPos + dir, len));
                        }

                        // Without Shift, the anchor follows the cursor; with Shift, the anchor stays fixed to select
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
                            // For Input Map action names (field 4), convert spaces to underscores automatically
                            char chToInsert = static_cast<char>(ch);
                            if (m_focusedTextFieldId == 4 && chToInsert == ' ') {
                                chToInsert = '_';
                            }

                            if (hasSelection() || targetStr->size() < 64) {
                                if (isEntityField) PushUndoState(scene);
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
                        if (isEntityField) PushUndoState(scene);
                        deleteSelection();
                    } else if (m_textCursorPos > 0) {
                        if (isEntityField) PushUndoState(scene);
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
                        if (isEntityField) PushUndoState(scene);
                        deleteSelection();
                    } else if (m_textCursorPos < curLen) {
                        if (isEntityField) PushUndoState(scene);
                        int eraseEnd = ctrlDown ? FindNextWordBoundary(*targetStr, m_textCursorPos, 1)
                                                : (m_textCursorPos + 1);
                        targetStr->erase(static_cast<size_t>(m_textCursorPos),
                                         static_cast<size_t>(eraseEnd - m_textCursorPos));
                        m_textSelectionAnchor = m_textCursorPos;
                    }
                }

                // 4. Submit or Unfocus on Enter / Escape
                if (Input::IsKeyPressed(Key::Enter)) {
                    if (m_focusedTextFieldId == 4 && !m_newActionNameBuffer.empty()) {
                        std::string actionName = m_newActionNameBuffer;
                        if (InputMap::AddAction(actionName)) {
                            InputMap::Save(m_inputMapFilePath);
                            ShowStatusMessage("Created action '" + actionName + "'");
                            m_newActionNameBuffer.clear();
                            m_textCursorPos = 0;
                            m_textSelectionAnchor = 0;
                            m_inputMapScrollOffset = 9999; // Scroll to bottom so new action is visible
                        } else {
                            ShowStatusMessage("Action '" + actionName + "' already exists!");
                        }
                    } else {
                        m_focusedTextFieldId = -1;
                        m_isDraggingText = false;
                    }
                } else if (Input::IsKeyPressed(Key::Escape)) {
                    m_focusedTextFieldId = -1;
                    m_isDraggingText = false;
                }
            }
        } else if (!m_isPlaying && m_focusedTextFieldId == -1 && !m_inputMapOpen) {
            // Allow deleting the selected entity with the Delete key when no text box is focused
            if (m_selectedEntityId != 0 && Input::IsKeyPressed(Key::Delete)) {
                PushUndoState(scene);
                const Entity* target = scene.FindEntityById(m_selectedEntityId);
                std::string deletedName = target ? target->name : std::to_string(m_selectedEntityId);
                scene.RemoveEntity(m_selectedEntityId);
                m_selectedEntityId = 0;
                TALOS_LOG_INFO("Deleted entity '" + deletedName + "' via keyboard shortcut.");
            }
            // Frame / Focus the selected entity in the center of the 2D camera when pressing 'F'
            if (!ctrlDown && Input::IsKeyPressed(Key::F)) {
                FocusSelectedEntity(scene);
            }
        }

        // Smooth frame delta time for readable FPS / frame time display in the F3 Diagnostic Overlay
        if (delta > 0.00001f && delta < 1.0f) {
            m_smoothedFrameDelta = m_smoothedFrameDelta * 0.90f + delta * 0.10f;
        }

        // In Play Mode, advance the real game simulation and allow F3 to toggle the Diagnostic Overlay.
        // In Edit Mode, F3 is ignored and the user can Pan/Zoom the camera & pick entities.
        if (m_isPlaying) {
            if (Input::IsKeyPressed(Key::F3)) {
                m_diagnosticOverlayEnabled = !m_diagnosticOverlayEnabled;
                ShowStatusMessage(m_diagnosticOverlayEnabled
                    ? "Viewport Diagnostics ON [F3]"
                    : "Viewport Diagnostics OFF [F3]");
                TALOS_LOG_INFO(std::string("Viewport Diagnostic Overlay (F3) ") +
                               (m_diagnosticOverlayEnabled ? "ENABLED" : "DISABLED"));
            }
            scene.Update(delta);
        } else if (!m_inputMapOpen) {
            HandleViewportInput(scene);
        }
    }

    float Editor::GetConsolePanelHeight() const {
        return m_consoleOpen ? CONSOLE_HEIGHT : CONSOLE_HEADER_HEIGHT;
    }

    void Editor::ResetCamera() {
        m_camera.targetX = 0.0f;
        m_camera.targetY = 0.0f;
        m_camera.offsetX = 0.0f;
        m_camera.offsetY = 0.0f;
        m_camera.zoom    = 1.0f;
        ShowStatusMessage("Camera Reset (100%)");
    }

    void Editor::FocusSelectedEntity(const Scene::Scene& scene) {
        const Entity* selected = const_cast<Scene::Scene&>(scene).FindEntityById(m_selectedEntityId);
        if (!selected) {
            ResetCamera();
            return;
        }

        float viewportX = LEFT_PANEL_WIDTH;
        float viewportY = TOOLBAR_HEIGHT;
        float viewportW = static_cast<float>(m_windowWidth) - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
        float viewportH = static_cast<float>(m_windowHeight) - TOOLBAR_HEIGHT - GetConsolePanelHeight();

        float viewportCenterX = viewportX + viewportW * 0.5f;
        float viewportCenterY = viewportY + viewportH * 0.5f;

        float entityCenterX = selected->transform.x + selected->transform.width * 0.5f;
        float entityCenterY = selected->transform.y + selected->transform.height * 0.5f;

        m_camera.offsetX = 0.0f;
        m_camera.offsetY = 0.0f;
        m_camera.targetX = entityCenterX - (viewportCenterX / m_camera.zoom);
        m_camera.targetY = entityCenterY - (viewportCenterY / m_camera.zoom);
        ShowStatusMessage("Focused on " + selected->name);
    }

    void Editor::HandleViewportInput(Scene::Scene& scene) {
        float viewportX = LEFT_PANEL_WIDTH;
        float viewportY = TOOLBAR_HEIGHT;
        float viewportW = static_cast<float>(m_windowWidth) - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
        float viewportH = static_cast<float>(m_windowHeight) - TOOLBAR_HEIGHT - GetConsolePanelHeight();

        float mx = Input::GetMouseX();
        float my = Input::GetMouseY();
        bool mouseInViewport = IsMouseOver(viewportX, viewportY, viewportW, viewportH);

        // Protect the bottom-left Camera Overlay HUD bar from triggering viewport entity clicks
        float hudX = viewportX + 12.0f;
        float hudY = static_cast<float>(m_windowHeight) - GetConsolePanelHeight() - 38.0f;
        float hudW = 240.0f;
        float hudH = 28.0f;
        bool mouseOverCameraHud = IsMouseOver(hudX, hudY, hudW, hudH);

        // =====================================================================
        // 1. Interactive 2D Camera Zoom (Mouse Wheel, Zoom-to-Cursor)
        // =====================================================================
        if (mouseInViewport) {
            float wheel = Input::GetMouseWheelMove();
            if (std::fabs(wheel) > 0.001f) {
                // Capture the exact world coordinate beneath the mouse cursor BEFORE zooming
                float worldBeforeX = 0.0f;
                float worldBeforeY = 0.0f;
                m_camera.ScreenToWorld(mx, my, worldBeforeX, worldBeforeY);

                // Apply multiplicative zoom step and clamp between 20% (0.20x) and 500% (5.00x)
                float zoomFactor = (wheel > 0.0f) ? 1.12f : (1.0f / 1.12f);
                m_camera.zoom = std::max(0.20f, std::min(5.00f, m_camera.zoom * zoomFactor));

                // Adjust targetX/targetY so the world point under the mouse cursor stays stationary
                m_camera.targetX = worldBeforeX - (mx - m_camera.offsetX) / m_camera.zoom;
                m_camera.targetY = worldBeforeY - (my - m_camera.offsetY) / m_camera.zoom;
            }
        }

        // =====================================================================
        // 2. Interactive 2D Camera Pan (Middle-Click, Right-Click, or Space+Left-Click)
        // =====================================================================
        bool spacePanInitiated = (m_focusedTextFieldId == -1) &&
                                 Input::IsKeyDown(Key::Space) &&
                                 Input::IsMouseButtonPressed(MouseButton::Left);

        if (mouseInViewport && !mouseOverCameraHud &&
            (Input::IsMouseButtonPressed(MouseButton::Middle) ||
             Input::IsMouseButtonPressed(MouseButton::Right) ||
             spacePanInitiated)) {
            m_isPanningCamera = true;
            m_panLastMouseX = mx;
            m_panLastMouseY = my;
            m_focusedTextFieldId = -1;
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

            // Unfocus any active Inspector text field when clicking the viewport
            m_focusedTextFieldId = -1;

            // Iterate backwards (front-to-back visually) so top-most entities are picked first
            auto& entities = scene.GetEntities();
            bool hitAny = false;
            for (int i = static_cast<int>(entities.size()) - 1; i >= 0; --i) {
                Entity& e = entities[i];
                if (!e.active) continue;

                // Compute clickable bounds in world coordinates (uses text dimensions if Sprite is disabled)
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
                    // Capture pre-drag scene snapshot before starting the drag operation
                    BeginContinuousEdit(scene);
                    m_selectedEntityId = e.id;
                    m_isDraggingEntity = true;
                    m_dragOffsetX = worldMx - e.transform.x;
                    m_dragOffsetY = worldMy - e.transform.y;
                    hitAny = true;
                    break;
                }
            }

            if (!hitAny) {
                m_selectedEntityId = 0;
            }
        }

        // Update world position of the dragged entity while holding the left mouse button
        if (m_isDraggingEntity && Input::IsMouseButtonDown(MouseButton::Left)) {
            Entity* selected = scene.FindEntityById(m_selectedEntityId);
            if (selected) {
                selected->transform.x = worldMx - m_dragOffsetX;
                selected->transform.y = worldMy - m_dragOffsetY;
            }
        }
    }

    void Editor::Render(Scene::Scene& scene) {
        m_activeScene = &scene;

        int vpX = static_cast<int>(LEFT_PANEL_WIDTH);
        int vpY = static_cast<int>(TOOLBAR_HEIGHT);
        int vpW = m_windowWidth - static_cast<int>(LEFT_PANEL_WIDTH + RIGHT_PANEL_WIDTH);
        int vpH = m_windowHeight - static_cast<int>(TOOLBAR_HEIGHT + GetConsolePanelHeight());

        // Clip all 2D world rendering strictly to the central viewport region
        Renderer2D::BeginViewportScissor(vpX, vpY, vpW, vpH);
        Renderer2D::BeginCamera2D(m_camera);

        // 1. Draw the infinite world-space reference grid and origin axes in Edit Mode
        if (!m_isPlaying) {
            RenderViewportGrid();
        }

        // 2. Render all entities in the Scene transformed by the 2D Camera
        scene.Render();

        // 3. Draw visual selection gizmo and collider indicators in Edit Mode
        if (!m_isPlaying) {
            RenderSelectionGizmo(scene);
        }

        Renderer2D::EndCamera2D();
        Renderer2D::EndViewportScissor();

        // 4. Draw Viewport HUD Overlays:
        //    - In Edit Mode: floating 2D Camera zoom/pan overlay (unless Input Map modal is open)
        //    - In Play Mode: real-time Per-Component Diagnostic Overlay (when toggled via F3)
        if (!m_isPlaying) {
            if (!m_inputMapOpen) {
                RenderViewportCameraOverlay(scene);
            }
        } else if (m_diagnosticOverlayEnabled) {
            RenderViewportDiagnosticOverlay(scene);
        }

        // 5. Render the Editor UI panels on top of the window
        RenderConsolePanel();
        RenderEntityListPanel(scene);
        RenderInspectorPanel(scene);
        RenderTopToolbar(scene);

        // 6. Render the Godot-style Project Input Map Modal Window on top when open
        if (!m_isPlaying && m_inputMapOpen) {
            RenderInputMapModal();
        }
    }

    void Editor::RenderViewportGrid() const {
        float vpLeft   = LEFT_PANEL_WIDTH;
        float vpRight  = static_cast<float>(m_windowWidth) - RIGHT_PANEL_WIDTH;
        float vpTop    = TOOLBAR_HEIGHT;
        float vpBottom = static_cast<float>(m_windowHeight) - GetConsolePanelHeight();

        // Convert screen viewport corners into visible 2D world coordinates
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

    void Editor::RenderSelectionGizmo(const Scene::Scene& scene) const {
        float invZoom = (m_camera.zoom > 0.01f) ? (1.0f / m_camera.zoom) : 1.0f;

        // Draw green collision bounds for entities with active colliders
        for (const auto& e : scene.GetEntities()) {
            if (!e.active) continue;
            if (e.collider.enabled) {
                Renderer2D::DrawRectOutline(
                    e.transform.x, e.transform.y,
                    e.transform.width, e.transform.height,
                    std::max(1.0f, 1.0f * invZoom), { 75, 220, 120, 140 }
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

        float pad = 3.0f * invZoom;
        Renderer2D::DrawRectOutline(
            bx - pad, by - pad, bw + pad * 2.0f, bh + pad * 2.0f,
            std::max(1.5f, 2.0f * invZoom), { 255, 215, 60, 255 }
        );
    }

    void Editor::RenderViewportCameraOverlay(const Scene::Scene& scene) {
        float hudX = LEFT_PANEL_WIDTH + 12.0f;
        float hudY = static_cast<float>(m_windowHeight) - GetConsolePanelHeight() - 38.0f;

        // Semi-transparent pill background for zoom controls
        Renderer2D::DrawRect(hudX, hudY, 232.0f, 28.0f, { 20, 22, 28, 220 });
        Renderer2D::DrawRectOutline(hudX, hudY, 232.0f, 28.0f, 1.0f, { 65, 70, 88, 220 });

        char zoomBuf[32];
        std::snprintf(zoomBuf, sizeof(zoomBuf), "Zoom: %.0f%%", m_camera.zoom * 100.0f);
        Renderer2D::DrawLabel(zoomBuf, hudX + 10.0f, hudY + 6.0f, 14, { 210, 220, 240, 255 });

        if (DrawButton(hudX + 98.0f, hudY + 3.0f, 60.0f, 22.0f, "Reset",
                       { 45, 50, 65, 255 }, { 65, 72, 94, 255 })) {
            ResetCamera();
        }
        if (DrawButton(hudX + 164.0f, hudY + 3.0f, 62.0f, 22.0f, "Focus [F]",
                       { 45, 85, 135, 255 }, { 60, 110, 175, 255 })) {
            FocusSelectedEntity(scene);
        }

        // Subtle navigation hint along the bottom of the viewport
        Renderer2D::DrawLabel(
            "Pan: Right/Middle Drag or Space+Drag   |   Zoom: Mouse Wheel",
            hudX + 244.0f, hudY + 7.0f, 14, { 125, 130, 148, 220 }
        );
    }

    // Helper: Formats an exact byte count into a compact human-readable string (B, KB, or MB).
    static void FormatBytes(size_t bytes, char* outBuf, size_t bufSize) {
        if (bytes < 1024) {
            std::snprintf(outBuf, bufSize, "%zu B", bytes);
        } else if (bytes < 1024 * 1024) {
            std::snprintf(outBuf, bufSize, "%.1f KB", static_cast<double>(bytes) / 1024.0);
        } else {
            std::snprintf(outBuf, bufSize, "%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        }
    }

    void Editor::RenderViewportDiagnosticOverlay(const Scene::Scene& scene) const {
        const SceneDiagnostics& diag = scene.GetDiagnostics();

        int vpX = static_cast<int>(LEFT_PANEL_WIDTH);
        int vpY = static_cast<int>(TOOLBAR_HEIGHT);
        int vpW = m_windowWidth - static_cast<int>(LEFT_PANEL_WIDTH + RIGHT_PANEL_WIDTH);
        int vpH = m_windowHeight - static_cast<int>(TOOLBAR_HEIGHT + GetConsolePanelHeight());
        if (vpW <= 40 || vpH <= 40) return;

        Renderer2D::BeginViewportScissor(vpX, vpY, vpW, vpH);

        float boxX = LEFT_PANEL_WIDTH + 10.0f;
        float boxY = TOOLBAR_HEIGHT + 10.0f;
        float boxW = std::min(680.0f, static_cast<float>(vpW) - 20.0f);
        float boxH = 222.0f;

        // Translucent dark telemetry card background & crisp cyan-slate border
        Renderer2D::DrawRect(boxX, boxY, boxW, boxH, { 14, 16, 22, 232 });
        Renderer2D::DrawRectOutline(boxX, boxY, boxW, boxH, 1.0f, { 70, 130, 180, 220 });

        // 1. Header Bar
        Renderer2D::DrawRect(boxX + 1.0f, boxY + 1.0f, boxW - 2.0f, 26.0f, { 22, 28, 38, 245 });
        Renderer2D::DrawLine2D(boxX, boxY + 27.0f, boxX + boxW, boxY + 27.0f, { 55, 85, 120, 220 });

        Renderer2D::DrawLabel(
            "VIEWPORT DIAGNOSTICS [F3]  -  COMPONENT CONSUMPTION",
            boxX + 10.0f, boxY + 6.0f, 14, { 95, 205, 255, 255 }
        );

        float fps = (m_smoothedFrameDelta > 0.0001f) ? (1.0f / m_smoothedFrameDelta) : 0.0f;
        float frameMs = m_smoothedFrameDelta * 1000.0f;
        char headerRight[96];
        std::snprintf(
            headerRight, sizeof(headerRight),
            "FPS: %.0f (%.2f ms)  |  Entities: %zu/%zu",
            fps, frameMs, diag.activeEntities, diag.totalEntities
        );
        int hrWidth = Renderer2D::MeasureTextWidth(headerRight, 14);
        Renderer2D::DrawLabel(
            headerRight,
            boxX + boxW - static_cast<float>(hrWidth) - 10.0f,
            boxY + 6.0f, 14, { 180, 235, 195, 255 }
        );

        // 2. Column Headers
        float colHeaderY = boxY + 32.0f;
        Renderer2D::DrawRect(boxX + 1.0f, colHeaderY - 3.0f, boxW - 2.0f, 20.0f, { 18, 21, 28, 240 });

        float colCompX  = boxX + 10.0f;
        float colInstX  = boxX + 122.0f;
        float colCpuX   = boxX + 196.0f;
        float colShareX = boxX + 318.0f;
        float colRamX   = boxX + 418.0f;
        float colVramX  = boxX + 534.0f;
        float colOpsX   = boxX + 606.0f;

        ColorRGBA colHdrColor = { 140, 150, 172, 255 };
        Renderer2D::DrawLabel("COMPONENT",      colCompX,  colHeaderY, 14, colHdrColor);
        Renderer2D::DrawLabel("ACTIVE",         colInstX,  colHeaderY, 14, colHdrColor);
        Renderer2D::DrawLabel("CPU TIME",       colCpuX,   colHeaderY, 14, colHdrColor);
        Renderer2D::DrawLabel("CPU LOAD",       colShareX, colHeaderY, 14, colHdrColor);
        Renderer2D::DrawLabel("RAM (ACT/TOT)",  colRamX,   colHeaderY, 14, colHdrColor);
        Renderer2D::DrawLabel("VRAM",           colVramX,  colHeaderY, 14, colHdrColor);
        Renderer2D::DrawLabel("OPS",            colOpsX,   colHeaderY, 14, colHdrColor);

        Renderer2D::DrawLine2D(boxX + 6.0f, colHeaderY + 18.0f, boxX + boxW - 6.0f, colHeaderY + 18.0f, { 40, 46, 60, 255 });

        // 3. Per-Component Consumption Rows
        struct RowEntry {
            const char* label;
            const ComponentDiagnostics* data;
            const char* opsUnit;
            ColorRGBA accent;
        };

        const RowEntry rows[5] = {
            { "Transform 2D", &diag.transform, "bounds", { 95,  195, 255, 255 } },
            { "Movement 2D",  &diag.movement,  "moving", { 105, 230, 145, 255 } },
            { "Collider 2D",  &diag.collider,  "AABB",   { 250, 200, 75,  255 } },
            { "Sprite 2D",    &diag.sprite,    "draws",  { 245, 130, 95,  255 } },
            { "Text Label",   &diag.text,      "draws",  { 195, 135, 255, 255 } }
        };

        double totalCompCpuUs = diag.transform.cpuTimeUs +
                                diag.movement.cpuTimeUs +
                                diag.collider.cpuTimeUs +
                                diag.sprite.cpuTimeUs +
                                diag.text.cpuTimeUs;
        size_t totalActiveRam = 0;
        size_t totalCompRam   = 0;
        size_t totalActiveVram = diag.sprite.vramBytes + diag.text.vramBytes;

        float rowY = colHeaderY + 21.0f;
        constexpr float rowH = 22.0f;

        for (int i = 0; i < 5; ++i) {
            const RowEntry& r = rows[i];
            const ComponentDiagnostics& cd = *r.data;

            totalActiveRam += cd.activeRamBytes;
            totalCompRam   += cd.ramBytes;

            // Alternating row background stripe
            ColorRGBA stripeBg = (i % 2 == 0)
                ? ColorRGBA{ 20, 23, 31, 210 }
                : ColorRGBA{ 16, 18, 25, 210 };
            Renderer2D::DrawRect(boxX + 4.0f, rowY, boxW - 8.0f, rowH - 1.0f, stripeBg);

            // Left color indicator bar + Component Name
            Renderer2D::DrawRect(colCompX - 4.0f, rowY + 3.0f, 3.0f, rowH - 7.0f, r.accent);
            Renderer2D::DrawLabel(r.label, colCompX + 3.0f, rowY + 3.0f, 14, r.accent);

            // Active / Total component instances
            char instBuf[32];
            std::snprintf(instBuf, sizeof(instBuf), "%zu / %zu", cd.activeCount, cd.totalCount);
            Renderer2D::DrawLabel(instBuf, colInstX, rowY + 3.0f, 14, { 220, 225, 238, 255 });

            // Exact CPU execution time in microseconds (and milliseconds)
            char cpuBuf[40];
            std::snprintf(cpuBuf, sizeof(cpuBuf), "%.2f us (%.3fms)", cd.cpuTimeUs, cd.cpuTimeUs / 1000.0);
            Renderer2D::DrawLabel(cpuBuf, colCpuX, rowY + 3.0f, 14, { 235, 240, 250, 255 });

            // Proportional CPU share bar + percentage
            float shareRatio = (totalCompCpuUs > 0.001)
                ? static_cast<float>(cd.cpuTimeUs / totalCompCpuUs)
                : 0.0f;
            shareRatio = std::max(0.0f, std::min(1.0f, shareRatio));

            float barW = 46.0f;
            float barH = 8.0f;
            float barY = rowY + 7.0f;
            Renderer2D::DrawRect(colShareX, barY, barW, barH, { 32, 36, 48, 255 });
            if (shareRatio > 0.01f) {
                Renderer2D::DrawRect(colShareX, barY, std::max(2.0f, barW * shareRatio), barH, r.accent);
            }
            char pctBuf[16];
            std::snprintf(pctBuf, sizeof(pctBuf), "%3.0f%%", shareRatio * 100.0f);
            Renderer2D::DrawLabel(pctBuf, colShareX + barW + 5.0f, rowY + 3.0f, 14, { 185, 195, 215, 255 });

            // Exact RAM consumption (Active / Total Bytes)
            char actRamStr[24], totRamStr[24], ramBuf[56];
            FormatBytes(cd.activeRamBytes, actRamStr, sizeof(actRamStr));
            FormatBytes(cd.ramBytes,       totRamStr, sizeof(totRamStr));
            std::snprintf(ramBuf, sizeof(ramBuf), "%s / %s", actRamStr, totRamStr);
            Renderer2D::DrawLabel(ramBuf, colRamX, rowY + 3.0f, 14, { 210, 220, 235, 255 });

            // Exact GPU VRAM consumption (Texture / Font Atlas)
            char vramBuf[24];
            FormatBytes(cd.vramBytes, vramBuf, sizeof(vramBuf));
            ColorRGBA vramColor = (cd.vramBytes > 0)
                ? ColorRGBA{ 125, 220, 255, 255 }
                : ColorRGBA{ 105, 112, 128, 255 };
            Renderer2D::DrawLabel(vramBuf, colVramX, rowY + 3.0f, 14, vramColor);

            // Operations performed this frame
            char opsBuf[32];
            std::snprintf(opsBuf, sizeof(opsBuf), "%zu %s", cd.operationsCount, r.opsUnit);
            Renderer2D::DrawLabel(opsBuf, colOpsX, rowY + 3.0f, 14, { 175, 185, 205, 255 });

            rowY += rowH;
        }

        // 4. Totals Summary Row
        Renderer2D::DrawLine2D(boxX + 6.0f, rowY + 1.0f, boxX + boxW - 6.0f, rowY + 1.0f, { 55, 85, 120, 200 });
        rowY += 4.0f;

        char totActiveRamStr[24], totSceneRamStr[24], totVramStr[24];
        FormatBytes(totalActiveRam,          totActiveRamStr, sizeof(totActiveRamStr));
        FormatBytes(diag.totalSceneRamBytes, totSceneRamStr,  sizeof(totSceneRamStr));
        FormatBytes(totalActiveVram,         totVramStr,      sizeof(totVramStr));

        char summaryLine[180];
        std::snprintf(
            summaryLine, sizeof(summaryLine),
            "TOTALS  |  Update: %.2f us  +  Render: %.2f us  =  %.2f us   |   Comp RAM: %s (Scene: %s)   |   VRAM: %s",
            diag.totalUpdateUs, diag.totalRenderUs,
            diag.totalUpdateUs + diag.totalRenderUs,
            totActiveRamStr, totSceneRamStr, totVramStr
        );
        Renderer2D::DrawLabel(summaryLine, boxX + 10.0f, rowY + 2.0f, 14, { 135, 225, 170, 255 });
        rowY += 21.0f;

        // 5. Selected Entity Exact Per-Component Breakdown Row
        Renderer2D::DrawRect(boxX + 4.0f, rowY, boxW - 8.0f, 22.0f, { 22, 26, 36, 235 });
        const Entity* sel = const_cast<Scene::Scene&>(scene).FindEntityById(m_selectedEntityId);
        if (sel) {
            size_t tRam = sizeof(TransformComponent);
            size_t mRam = sizeof(MovementComponent);
            size_t cRam = sizeof(ColliderComponent);
            size_t sRam = sizeof(SpriteComponent) + sel->sprite.texturePath.capacity();
            size_t xRam = sizeof(TextComponent)   + sel->text.content.capacity();
            size_t sVram = (sel->sprite.enabled && !sel->sprite.texturePath.empty())
                ? Renderer2D::GetTextureVRAMBytes(sel->sprite.texturePath.c_str())
                : 0;
            size_t xVram = sel->text.enabled
                ? Renderer2D::GetFontAtlasVRAMBytes(sel->text.fontSize)
                : 0;

            char sVramStr[24], xVramStr[24];
            FormatBytes(sVram, sVramStr, sizeof(sVramStr));
            FormatBytes(xVram, xVramStr, sizeof(xVramStr));

            char entityLine[220];
            std::snprintf(
                entityLine, sizeof(entityLine),
                "Entity '%s' (#%u): Trans %zuB | Move %zuB(%s) | Coll %zuB(%s) | Sprite %zuB+%s VRAM(%s) | Text %zuB+%s(%s)",
                sel->name.c_str(), sel->id,
                tRam,
                mRam, sel->movement.enabled ? "ON" : "OFF",
                cRam, sel->collider.enabled ? "ON" : "OFF",
                sRam, sVramStr, sel->sprite.enabled ? "ON" : "OFF",
                xRam, xVramStr, sel->text.enabled   ? "ON" : "OFF"
            );
            Renderer2D::DrawLabel(entityLine, boxX + 10.0f, rowY + 3.0f, 14, { 255, 215, 90, 255 });
        } else {
            Renderer2D::DrawLabel(
                "Selected Entity: None (Select an entity in the Hierarchy to inspect its exact per-component byte footprint)",
                boxX + 10.0f, rowY + 3.0f, 14, { 145, 152, 170, 255 }
            );
        }

        Renderer2D::EndViewportScissor();
    }

    void Editor::RenderInputMapModal() {
        float vpX = LEFT_PANEL_WIDTH;
        float vpY = TOOLBAR_HEIGHT;
        float vpW = static_cast<float>(m_windowWidth) - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
        float vpH = static_cast<float>(m_windowHeight) - TOOLBAR_HEIGHT - GetConsolePanelHeight();

        // 1. Translucent dark backdrop over the central viewport
        Renderer2D::DrawRect(vpX, vpY, vpW, vpH, { 10, 11, 16, 200 });

        // Centered modal window dimensions
        float modalW = std::min(720.0f, static_cast<float>(m_windowWidth) - 80.0f);
        float modalH = std::min(490.0f, static_cast<float>(m_windowHeight) - TOOLBAR_HEIGHT - 40.0f);
        float modalX = (static_cast<float>(m_windowWidth) - modalW) * 0.5f;
        float modalY = TOOLBAR_HEIGHT + std::max(14.0f, (static_cast<float>(m_windowHeight) - TOOLBAR_HEIGHT - modalH) * 0.42f);

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
        if (DrawButton(modalX + modalW - 208.0f, modalY + 6.0f, 116.0f, 24.0f, "Reset Defaults",
                       { 55, 60, 78, 255 }, { 75, 82, 106, 255 })) {
            InputMap::InitDefaults();
            InputMap::Save(m_inputMapFilePath);
            m_listeningActionName.clear();
            m_inputMapScrollOffset = 0;
            ShowStatusMessage("Reset Input Map to default video game actions");
        }

        if (DrawButton(modalX + modalW - 84.0f, modalY + 6.0f, 72.0f, 24.0f, "Close [X]",
                       { 140, 48, 54, 255 }, { 175, 64, 70, 255 })) {
            m_inputMapOpen = false;
            m_listeningActionName.clear();
            if (m_focusedTextFieldId == 4) {
                m_focusedTextFieldId = -1;
            }
            return;
        }

        // 3. Add New Action Bar (Godot-style top input row)
        float addRowY = modalY + headerH + 10.0f;
        DrawTextInputBox(modalX + 14.0f, addRowY, modalW - 154.0f, "Action:", m_newActionNameBuffer, 4);

        if (DrawButton(modalX + modalW - 130.0f, addRowY - 1.0f, 116.0f, 25.0f, "+ Add Action",
                       { 42, 140, 82, 255 }, { 56, 170, 102, 255 })) {
            if (!m_newActionNameBuffer.empty()) {
                std::string actionName = m_newActionNameBuffer;
                if (InputMap::AddAction(actionName)) {
                    InputMap::Save(m_inputMapFilePath);
                    ShowStatusMessage("Created action '" + actionName + "'");
                    m_newActionNameBuffer.clear();
                    m_textCursorPos = 0;
                    m_textSelectionAnchor = 0;
                    m_inputMapScrollOffset = 9999; // Scroll to bottom so new action is immediately visible
                } else {
                    ShowStatusMessage("Action '" + actionName + "' already exists!");
                }
            } else {
                m_focusedTextFieldId = 4;
                ShowStatusMessage("Type an action name first!");
            }
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

        if (IsMouseOver(modalX, listY, modalW, listH)) {
            float wheel = Input::GetMouseWheelMove();
            if (wheel > 0.01f) {
                m_inputMapScrollOffset = std::max(0, m_inputMapScrollOffset - 1);
            } else if (wheel < -0.01f) {
                m_inputMapScrollOffset = std::min(maxScroll, m_inputMapScrollOffset + 1);
            }
        }
        m_inputMapScrollOffset = std::max(0, std::min(m_inputMapScrollOffset, maxScroll));

        // Deferred mutations so vector iterators are never invalidated mid-render
        std::string actionToDelete;
        std::string actionToUnbind;
        Key keyToUnbind = Key::Unknown;

        Renderer2D::BeginViewportScissor(
            static_cast<int>(modalX + 2.0f),
            static_cast<int>(listY),
            static_cast<int>(modalW - 4.0f),
            static_cast<int>(listH)
        );

        int startIdx = m_inputMapScrollOffset;
        int endIdx   = std::min(totalRows, startIdx + maxVisibleRows);
        float rowY   = listY + 3.0f;

        for (int i = startIdx; i < endIdx; ++i) {
            const InputAction& action = actions[static_cast<size_t>(i)];
            bool isListeningThis = (m_listeningActionName == action.name);
            bool isHeldNow       = InputMap::IsActionDown(action.name);

            // Row background stripe (highlighted if currently listening for a key or held down)
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
                    ColorRGBA chipHover = { 155, 52, 58, 255 }; // Turns red on hover to indicate unbind

                    if (DrawButton(keyCursorX, rowY + 3.0f, chipW, 22.0f, chipLabel.c_str(), chipBg, chipHover)) {
                        actionToUnbind = action.name;
                        keyToUnbind = k;
                    }
                    keyCursorX += chipW + 6.0f;
                }
            }

            // Right-side Manage Buttons: "+ Key" (or "Press Key...") and "Del"
            if (isListeningThis) {
                if (DrawButton(colManageX, rowY + 3.0f, 92.0f, 22.0f, "Press Key...",
                               { 205, 135, 32, 255 }, { 225, 155, 48, 255 })) {
                    m_listeningActionName.clear();
                }
            } else {
                if (DrawButton(colManageX, rowY + 3.0f, 92.0f, 22.0f, "+ Bind Key",
                               { 48, 105, 185, 255 }, { 65, 130, 220, 255 })) {
                    m_listeningActionName = action.name;
                    m_focusedTextFieldId = -1;
                }
            }

            if (DrawButton(colManageX + 98.0f, rowY + 3.0f, 44.0f, 22.0f, "Del",
                           { 125, 45, 50, 255 }, { 165, 60, 66, 255 })) {
                actionToDelete = action.name;
            }

            rowY += rowH;
        }

        // Scrollbar thumb on the right edge of the modal list
        if (totalRows > maxVisibleRows) {
            float trackX = modalX + modalW - 9.0f;
            float trackY = listY + 3.0f;
            float trackH = listH - 6.0f;
            Renderer2D::DrawRect(trackX, trackY, 4.0f, trackH, { 32, 35, 44, 255 });

            float thumbRatio = static_cast<float>(maxVisibleRows) / static_cast<float>(totalRows);
            float thumbH = std::max(18.0f, trackH * thumbRatio);
            float scrollFraction = static_cast<float>( m_inputMapScrollOffset ) / static_cast<float>(maxScroll);
            float thumbY = trackY + (trackH - thumbH) * scrollFraction;
            Renderer2D::DrawRect(trackX, thumbY, 4.0f, thumbH, { 95, 145, 215, 255 });
        }

        Renderer2D::EndViewportScissor();

        // Execute deferred unbind / delete actions after iterating
        if (!actionToUnbind.empty() && keyToUnbind != Key::Unknown) {
            if (InputMap::UnbindKey(actionToUnbind, keyToUnbind)) {
                InputMap::Save(m_inputMapFilePath);
                ShowStatusMessage("Unbound [" + std::string(Input::GetKeyName(keyToUnbind)) +
                                  "] from '" + actionToUnbind + "'");
            }
        }
        if (!actionToDelete.empty()) {
            if (m_listeningActionName == actionToDelete) {
                m_listeningActionName.clear();
            }
            if (InputMap::RemoveAction(actionToDelete)) {
                InputMap::Save(m_inputMapFilePath);
                ShowStatusMessage("Removed action '" + actionToDelete + "'");
            }
        }

        // 6. Modal Footer Bar (shows listening prompt or C++ query hint)
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
                actions.size(), m_inputMapFilePath.c_str()
            );
            Renderer2D::DrawLabel(footerBuf, modalX + 14.0f, footerY + 7.0f, 14, { 145, 155, 178, 255 });
        }
    }

    void Editor::RenderConsolePanel() {
        float consoleX = LEFT_PANEL_WIDTH;
        float consoleW = static_cast<float>(m_windowWidth) - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
        float consoleH = GetConsolePanelHeight();
        float consoleY = static_cast<float>(m_windowHeight) - consoleH;

        // Console panel background & top divider border
        Renderer2D::DrawRect(consoleX, consoleY, consoleW, consoleH, { 18, 19, 24, 255 });
        Renderer2D::DrawLine2D(consoleX, consoleY, consoleX + consoleW, consoleY, { 58, 62, 76, 255 });

        // Header bar background
        Renderer2D::DrawRect(consoleX, consoleY, consoleW, CONSOLE_HEADER_HEIGHT, { 24, 26, 33, 255 });
        Renderer2D::DrawLine2D(consoleX, consoleY + CONSOLE_HEADER_HEIGHT,
                               consoleX + consoleW, consoleY + CONSOLE_HEADER_HEIGHT, { 42, 45, 56, 255 });

        // Title label
        Renderer2D::DrawLabel("DEBUG CONSOLE", consoleX + 12.0f, consoleY + 6.0f, 15, { 155, 165, 185, 255 });

        // Counts by message classification type
        size_t infoCount  = Core::Logger::GetCountByLevel(Core::LogLevel::Info);
        size_t warnCount  = Core::Logger::GetCountByLevel(Core::LogLevel::Warning);
        size_t errorCount = Core::Logger::GetCountByLevel(Core::LogLevel::Error);

        char infoBtnLabel[32];
        char warnBtnLabel[32];
        char errBtnLabel[32];
        std::snprintf(infoBtnLabel, sizeof(infoBtnLabel), "INFO: %zu", infoCount);
        std::snprintf(warnBtnLabel, sizeof(warnBtnLabel), "WARN: %zu", warnCount);
        std::snprintf(errBtnLabel,  sizeof(errBtnLabel),  "ERROR: %zu", errorCount);

        float filterX = consoleX + 148.0f;
        if (DrawButton(filterX, consoleY + 3.0f, 82.0f, 22.0f, infoBtnLabel,
                       m_showLogInfo ? ColorRGBA{ 38, 88, 138, 255 } : ColorRGBA{ 32, 35, 44, 255 },
                       { 52, 112, 172, 255 })) {
            m_showLogInfo = !m_showLogInfo;
        }

        if (DrawButton(filterX + 88.0f, consoleY + 3.0f, 86.0f, 22.0f, warnBtnLabel,
                       m_showLogWarn ? ColorRGBA{ 135, 95, 28, 255 } : ColorRGBA{ 32, 35, 44, 255 },
                       { 168, 120, 38, 255 })) {
            m_showLogWarn = !m_showLogWarn;
        }

        if (DrawButton(filterX + 180.0f, consoleY + 3.0f, 92.0f, 22.0f, errBtnLabel,
                       m_showLogError ? ColorRGBA{ 148, 42, 48, 255 } : ColorRGBA{ 32, 35, 44, 255 },
                       { 182, 56, 62, 255 })) {
            m_showLogError = !m_showLogError;
        }

        // Right-aligned Clear and Collapse/Expand buttons
        float rightBtnX = consoleX + consoleW - 142.0f;
        if (DrawButton(rightBtnX, consoleY + 3.0f, 64.0f, 22.0f, "Clear",
                       { 45, 48, 60, 255 }, { 65, 70, 88, 255 })) {
            Core::Logger::Clear();
            m_consoleScrollOffset = 0;
            m_lastLogCount = 0;
        }

        if (DrawButton(rightBtnX + 70.0f, consoleY + 3.0f, 62.0f, 22.0f,
                       m_consoleOpen ? "Hide" : "Show",
                       { 45, 48, 60, 255 }, { 65, 70, 88, 255 })) {
            m_consoleOpen = !m_consoleOpen;
        }

        if (!m_consoleOpen) {
            return;
        }

        // Gather filtered entries
        const auto& allEntries = Core::Logger::GetEntries();
        std::vector<const Core::LogEntry*> filtered;
        filtered.reserve(allEntries.size());
        for (const auto& entry : allEntries) {
            if (entry.level == Core::LogLevel::Info    && !m_showLogInfo)  continue;
            if (entry.level == Core::LogLevel::Warning && !m_showLogWarn)  continue;
            if (entry.level == Core::LogLevel::Error   && !m_showLogError) continue;
            filtered.push_back(&entry);
        }

        // Auto-scroll to the newest message at the bottom when new logs arrive
        if (allEntries.size() != m_lastLogCount) {
            m_consoleScrollOffset = 0;
            m_lastLogCount = allEntries.size();
        }

        float bodyY = consoleY + CONSOLE_HEADER_HEIGHT;
        float bodyH = consoleH - CONSOLE_HEADER_HEIGHT;
        constexpr float rowHeight = 22.0f;
        int maxVisibleRows = std::max(1, static_cast<int>((bodyH - 6.0f) / rowHeight));
        int totalRows = static_cast<int>(filtered.size());
        int maxScroll = std::max(0, totalRows - maxVisibleRows);

        // Mouse wheel scrolling when hovering over the console log body
        if (IsMouseOver(consoleX, bodyY, consoleW, bodyH)) {
            float wheel = Input::GetMouseWheelMove();
            if (wheel > 0.01f) {
                m_consoleScrollOffset = std::min(maxScroll, m_consoleScrollOffset + 1);
            } else if (wheel < -0.01f) {
                m_consoleScrollOffset = std::max(0, m_consoleScrollOffset - 1);
            }
        }
        m_consoleScrollOffset = std::max(0, std::min(m_consoleScrollOffset, maxScroll));

        // Clip log rows strictly to the console body area
        Renderer2D::BeginViewportScissor(
            static_cast<int>(consoleX),
            static_cast<int>(bodyY),
            static_cast<int>(consoleW),
            static_cast<int>(bodyH)
        );

        if (filtered.empty()) {
            Renderer2D::DrawLabel(
                "No messages matching current filter.",
                consoleX + 14.0f, bodyY + 10.0f, 14, { 110, 115, 130, 255 }
            );
        } else {
            int endIdx   = totalRows - m_consoleScrollOffset;
            int startIdx = std::max(0, endIdx - maxVisibleRows);

            float rowY = bodyY + 3.0f;
            for (int i = startIdx; i < endIdx; ++i) {
                const Core::LogEntry& entry = *filtered[i];

                // Alternating subtle row background + red/amber tint for Errors and Warnings
                ColorRGBA rowBg = (i % 2 == 0) ? ColorRGBA{ 22, 24, 30, 255 } : ColorRGBA{ 18, 19, 24, 255 };
                if (entry.level == Core::LogLevel::Error) {
                    rowBg = { 48, 20, 24, 255 };
                } else if (entry.level == Core::LogLevel::Warning) {
                    rowBg = { 44, 34, 18, 255 };
                }
                Renderer2D::DrawRect(consoleX + 2.0f, rowY, consoleW - 10.0f, rowHeight - 1.0f, rowBg);

                // 1. Classification Badge ([INFO], [WARN], [ERROR])
                ColorRGBA badgeColor = { 85, 190, 255, 255 };
                const char* badgeText = "[INFO]";
                if (entry.level == Core::LogLevel::Warning) {
                    badgeColor = { 255, 195, 65, 255 };
                    badgeText  = "[WARN]";
                } else if (entry.level == Core::LogLevel::Error) {
                    badgeColor = { 255, 95, 95, 255 };
                    badgeText  = "[ERROR]";
                }

                float curX = consoleX + 10.0f;
                Renderer2D::DrawLabel(badgeText, curX, rowY + 3.0f, 14, badgeColor);
                curX += 64.0f;

                // 2. Timestamp (HH:MM:SS)
                Renderer2D::DrawLabel(entry.timestamp.c_str(), curX, rowY + 3.0f, 14, { 120, 126, 142, 255 });
                curX += 66.0f;

                // 3. Source File & Line Number ([file:line])
                std::string locationTag = "[" + entry.file + ":" + std::to_string(entry.line) + "]";
                Renderer2D::DrawLabel(locationTag.c_str(), curX, rowY + 3.0f, 14, { 165, 180, 215, 255 });
                int locWidth = Renderer2D::MeasureTextWidth(locationTag.c_str(), 14);
                curX += static_cast<float>(locWidth) + 10.0f;

                // 4. Diagnostic Message Content
                ColorRGBA msgColor = { 230, 232, 240, 255 };
                if (entry.level == Core::LogLevel::Error) {
                    msgColor = { 255, 185, 185, 255 };
                } else if (entry.level == Core::LogLevel::Warning) {
                    msgColor = { 255, 228, 165, 255 };
                }
                Renderer2D::DrawLabel(entry.message.c_str(), curX, rowY + 3.0f, 14, msgColor);

                rowY += rowHeight;
            }

            // Draw vertical scrollbar indicator on the right edge if there are more rows than fit
            if (totalRows > maxVisibleRows) {
                float trackX = consoleX + consoleW - 6.0f;
                float trackY = bodyY + 2.0f;
                float trackH = bodyH - 4.0f;
                Renderer2D::DrawRect(trackX, trackY, 4.0f, trackH, { 30, 32, 40, 255 });

                float thumbRatio = static_cast<float>(maxVisibleRows) / static_cast<float>(totalRows);
                float thumbH = std::max(16.0f, trackH * thumbRatio);
                float scrollFraction = static_cast<float>(maxScroll - m_consoleScrollOffset) / static_cast<float>(maxScroll);
                float thumbY = trackY + (trackH - thumbH) * scrollFraction;
                Renderer2D::DrawRect(trackX, thumbY, 4.0f, thumbH, { 95, 105, 130, 255 });
            }
        }

        Renderer2D::EndViewportScissor();
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
        float centerBtnX = (w * 0.5f) - 210.0f;
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
            Renderer2D::DrawLabel("MODE: PLAYING LIVE", centerBtnX + 132.0f, 15.0f, 16, { 95, 230, 135, 255 });
            Renderer2D::DrawLabel(
                m_diagnosticOverlayEnabled ? "[F3] Diagnostics: ON" : "[F3] Diagnostics: OFF",
                centerBtnX + 296.0f, 15.0f, 15,
                m_diagnosticOverlayEnabled ? ColorRGBA{ 95, 205, 255, 255 } : ColorRGBA{ 130, 138, 158, 255 }
            );
        }

        // Entity Management, Input Map & Scene Serialization Buttons (only active in Edit Mode)
        if (!m_isPlaying) {
            if (DrawButton(LEFT_PANEL_WIDTH + 12.0f, 8.0f, 115.0f, 30.0f, "+ New Entity",
                           { 55, 115, 200, 255 }, { 75, 140, 230, 255 })) {
                PushUndoState(scene);
                char nameBuf[32];
                std::snprintf(nameBuf, sizeof(nameBuf), "Object_%d", m_newEntityCounter++);

                // Spawn the new entity right at the center of the currently visible 2D camera viewport
                float vpCenterX = LEFT_PANEL_WIDTH + (w - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH) * 0.5f;
                float vpCenterY = TOOLBAR_HEIGHT + (static_cast<float>(m_windowHeight) - TOOLBAR_HEIGHT - GetConsolePanelHeight()) * 0.5f;
                float spawnWorldX = 480.0f, spawnWorldY = 320.0f;
                m_camera.ScreenToWorld(vpCenterX, vpCenterY, spawnWorldX, spawnWorldY);

                Entity& created = scene.CreateEntity(nameBuf, spawnWorldX - 32.0f, spawnWorldY - 32.0f);
                created.sprite.color = PRESET_COLORS[m_newEntityCounter % NUM_PRESET_COLORS];
                m_selectedEntityId = created.id;
                TALOS_LOG_INFO("Created new entity '" + created.name + "' (id=" + std::to_string(created.id) + ").");
            }

            if (m_selectedEntityId != 0) {
                if (DrawButton(LEFT_PANEL_WIDTH + 136.0f, 8.0f, 85.0f, 30.0f, "Delete",
                               { 140, 50, 55, 255 }, { 180, 65, 70, 255 })) {
                    PushUndoState(scene);
                    const Entity* target = scene.FindEntityById(m_selectedEntityId);
                    std::string deletedName = target ? target->name : std::to_string(m_selectedEntityId);
                    scene.RemoveEntity(m_selectedEntityId);
                    m_selectedEntityId = 0;
                    TALOS_LOG_INFO("Deleted entity '" + deletedName + "'.");
                }
            }

            // Right-side Input Map, Scene Save & Load buttons
            float rightActionsX = w - RIGHT_PANEL_WIDTH - 228.0f;
            if (DrawButton(rightActionsX, 8.0f, 104.0f, 30.0f, "Input Map",
                           m_inputMapOpen ? ColorRGBA{ 55, 130, 220, 255 } : ColorRGBA{ 92, 68, 160, 255 },
                           m_inputMapOpen ? ColorRGBA{ 75, 155, 245, 255 } : ColorRGBA{ 115, 88, 192, 255 })) {
                m_inputMapOpen = !m_inputMapOpen;
                m_listeningActionName.clear();
                if (!m_inputMapOpen && m_focusedTextFieldId == 4) {
                    m_focusedTextFieldId = -1;
                }
            }

            if (DrawButton(rightActionsX + 112.0f, 8.0f, 104.0f, 30.0f, "Save Scene",
                           { 48, 125, 115, 255 }, { 65, 155, 142, 255 })) {
                if (SceneSerializer::Save(scene, m_sceneFilePath)) {
                    ShowStatusMessage("Saved -> " + m_sceneFilePath);
                } else {
                    ShowStatusMessage("Save failed!");
                }
            }

            if (DrawButton(rightActionsX + 224.0f, 8.0f, 104.0f, 30.0f, "Load Scene",
                           { 70, 75, 95, 255 }, { 92, 98, 122, 255 })) {
                PushUndoState(scene);
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
            if (m_activeScene) {
                PushUndoState(*m_activeScene);
            }
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
            if (m_activeScene) {
                BeginContinuousEdit(*m_activeScene);
            }
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
                if (!isCurrent && m_activeScene) {
                    PushUndoState(*m_activeScene);
                }

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
                    if (!AreColorsEqual(color, PRESET_COLORS[i]) && m_activeScene) {
                        PushUndoState(*m_activeScene);
                    }
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
        float textStartX = boxX + 8.0f;
        constexpr int fontSize = 14;

        bool hovered = IsMouseOver(boxX, y, boxW, boxH);

        // Begin click or mouse-drag selection inside the text box
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

        // Continuously update the active cursor index while dragging the mouse across the text
        if (isFocused && m_isDraggingText && Input::IsMouseButtonDown(MouseButton::Left)) {
            m_textCursorPos = GetTextCharIndexAtMouseX(textBuffer, textStartX, Input::GetMouseX(), fontSize);
        }

        // Draw input box background and border
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

        // Draw selection highlight rectangle behind the text if a range is selected
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

        // Draw the text content
        Renderer2D::DrawLabel(textBuffer.c_str(), textStartX, y + 4.0f, fontSize, { 245, 245, 250, 255 });

        // Draw the vertical caret line at the active cursor position
        if (isFocused) {
            int len = static_cast<int>(textBuffer.size());
            int cur = std::max(0, std::min(m_textCursorPos, len));
            std::string prefixCursor = textBuffer.substr(0, static_cast<size_t>(cur));
            float cursorX = textStartX + static_cast<float>(Renderer2D::MeasureTextWidth(prefixCursor.c_str(), fontSize));

            Renderer2D::DrawRect(cursorX, y + 4.0f, 2.0f, boxH - 8.0f, { 110, 210, 255, 255 });
        }
    }

}
