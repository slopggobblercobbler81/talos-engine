#include "editor/editor.h"
#include "scene/scene_serializer.h"
#include "core/input.h"
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

    // Helper: Returns true if the character belongs to a word (alphanumeric).
    // Used by Ctrl+Left / Ctrl+Right word-by-word navigation.
    static bool IsWordChar(char c) {
        return (c >= 'a' && c <= 'z') ||
               (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9');
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
        : m_windowWidth(windowWidth), m_windowHeight(windowHeight), m_sceneFilePath(sceneFilePath) {}

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
    }

    void Editor::Redo(Scene::Scene& scene) {
        if (m_isPlaying) return;

        if (m_redoStack.empty()) {
            ShowStatusMessage("Nothing to redo");
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

        // Handle keyboard typing, cursor navigation, and text selection when an Inspector text box is focused
        if (m_focusedTextFieldId != -1 && !m_isPlaying) {
            Entity* selected = scene.FindEntityById(m_selectedEntityId);
            if (selected) {
                std::string* targetStr = nullptr;
                if (m_focusedTextFieldId == 1) targetStr = &selected->name;
                if (m_focusedTextFieldId == 2) targetStr = &selected->text.content;
                if (m_focusedTextFieldId == 3) targetStr = &selected->sprite.texturePath;

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
                                if (hasSelection() || targetStr->size() < 64) {
                                    PushUndoState(scene);
                                    if (hasSelection()) {
                                        deleteSelection();
                                    }
                                    targetStr->insert(static_cast<size_t>(m_textCursorPos), 1, static_cast<char>(ch));
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
                            PushUndoState(scene);
                            deleteSelection();
                        } else if (m_textCursorPos > 0) {
                            PushUndoState(scene);
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
                            PushUndoState(scene);
                            deleteSelection();
                        } else if (m_textCursorPos < curLen) {
                            PushUndoState(scene);
                            int eraseEnd = ctrlDown ? FindNextWordBoundary(*targetStr, m_textCursorPos, 1)
                                                    : (m_textCursorPos + 1);
                            targetStr->erase(static_cast<size_t>(m_textCursorPos),
                                             static_cast<size_t>(eraseEnd - m_textCursorPos));
                            m_textSelectionAnchor = m_textCursorPos;
                        }
                    }

                    // 4. Unfocus on Enter or Escape
                    if (Input::IsKeyPressed(Key::Enter) || Input::IsKeyPressed(Key::Escape)) {
                        m_focusedTextFieldId = -1;
                        m_isDraggingText = false;
                    }
                }
            }
        } else if (!m_isPlaying && m_focusedTextFieldId == -1) {
            // Allow deleting the selected entity with the Delete key when no text box is focused
            if (m_selectedEntityId != 0 && Input::IsKeyPressed(Key::Delete)) {
                PushUndoState(scene);
                scene.RemoveEntity(m_selectedEntityId);
                m_selectedEntityId = 0;
            }
            // Frame / Focus the selected entity in the center of the 2D camera when pressing 'F'
            if (!ctrlDown && Input::IsKeyPressed(Key::F)) {
                FocusSelectedEntity(scene);
            }
        }

        // In Play Mode, advance the real game simulation; in Edit Mode, allow camera Pan/Zoom & mouse picking
        if (m_isPlaying) {
            scene.Update(delta);
        } else {
            HandleViewportInput(scene);
        }
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
        float viewportH = static_cast<float>(m_windowHeight) - TOOLBAR_HEIGHT;

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
        float viewportH = static_cast<float>(m_windowHeight) - TOOLBAR_HEIGHT;

        float mx = Input::GetMouseX();
        float my = Input::GetMouseY();
        bool mouseInViewport = IsMouseOver(viewportX, viewportY, viewportW, viewportH);

        // Protect the bottom-left Camera Overlay HUD bar from triggering viewport entity clicks
        float hudX = viewportX + 12.0f;
        float hudY = static_cast<float>(m_windowHeight) - 38.0f;
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
        int vpH = m_windowHeight - static_cast<int>(TOOLBAR_HEIGHT);

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

        // 4. Draw the floating 2D Camera HUD overlay inside the bottom-left of the viewport
        if (!m_isPlaying) {
            RenderViewportCameraOverlay(scene);
        }

        // 5. Render the Editor UI panels on top of the window
        RenderEntityListPanel(scene);
        RenderInspectorPanel(scene);
        RenderTopToolbar(scene);
    }

    void Editor::RenderViewportGrid() const {
        float vpLeft   = LEFT_PANEL_WIDTH;
        float vpRight  = static_cast<float>(m_windowWidth) - RIGHT_PANEL_WIDTH;
        float vpTop    = TOOLBAR_HEIGHT;
        float vpBottom = static_cast<float>(m_windowHeight);

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
        float hudY = static_cast<float>(m_windowHeight) - 38.0f;

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
                PushUndoState(scene);
                char nameBuf[32];
                std::snprintf(nameBuf, sizeof(nameBuf), "Object_%d", m_newEntityCounter++);

                // Spawn the new entity right at the center of the currently visible 2D camera viewport
                float vpCenterX = LEFT_PANEL_WIDTH + (w - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH) * 0.5f;
                float vpCenterY = TOOLBAR_HEIGHT + (static_cast<float>(m_windowHeight) - TOOLBAR_HEIGHT) * 0.5f;
                float spawnWorldX = 480.0f, spawnWorldY = 320.0f;
                m_camera.ScreenToWorld(vpCenterX, vpCenterY, spawnWorldX, spawnWorldY);

                Entity& created = scene.CreateEntity(nameBuf, spawnWorldX - 32.0f, spawnWorldY - 32.0f);
                created.sprite.color = PRESET_COLORS[m_newEntityCounter % NUM_PRESET_COLORS];
                m_selectedEntityId = created.id;
            }

            if (m_selectedEntityId != 0) {
                if (DrawButton(LEFT_PANEL_WIDTH + 136.0f, 8.0f, 85.0f, 30.0f, "Delete",
                               { 140, 50, 55, 255 }, { 180, 65, 70, 255 })) {
                    PushUndoState(scene);
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
