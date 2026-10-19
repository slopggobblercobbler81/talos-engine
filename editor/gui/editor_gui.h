// EditorGui declaration: immediate-mode widgets, text fields and component controls (implemented across editor_gui*.cpp)
#pragma once
#include <string>
#include "scene/scene.h"
#include "renderer/renderer2d.h"
#include "editor/history/editor_undo_redo.h"

namespace Talos::Editor {

    // =========================================================================
    // Editor Layout Metrics & Preset Palettes (Layer 5: Editor -> GUI Branch)
    // =========================================================================
    namespace EditorLayout {
        constexpr float TOOLBAR_HEIGHT        = 46.0f;
        constexpr float LEFT_PANEL_WIDTH      = 230.0f;
        constexpr float RIGHT_PANEL_WIDTH     = 350.0f;
        constexpr float CONSOLE_HEIGHT        = 185.0f;
        constexpr float CONSOLE_HEADER_HEIGHT = 28.0f;
    }

    struct TexturePreset {
        const char* label;
        const char* path;
    };

    // =========================================================================
    // Immediate-Mode Editor GUI Toolkit (Layer 5: Editor -> GUI Branch)
    // =========================================================================
    // Provides reusable UI controls (buttons, checkboxes, sliders, color pickers,
    // texture selectors, and rich text input boxes with caret/selection/word-jump)
    // built strictly on Core::Input and Renderer::Renderer2D.
    class EditorGui {
    public:
        static constexpr int NUM_PRESET_COLORS   = 8;
        static constexpr int NUM_TEXTURE_PRESETS = 6;

        static const Renderer::ColorRGBA PRESET_COLORS[NUM_PRESET_COLORS];
        static const TexturePreset TEXTURE_PRESETS[NUM_TEXTURE_PRESETS];

        // Binds the current frame's active Scene and Undo/Redo manager so widgets
        // automatically record Undo snapshots before mutating entity properties.
        void SetFrameContext(Scene::Scene* activeScene,
                             EditorUndoRedo* undoRedo,
                             Scene::EntityID selectedEntityId,
                             bool isPlaying);

        // Releases active slider or text selection drags when the left mouse button is released.
        void OnLeftMouseReleased();

        // Processes keyboard typing, cursor navigation, word-jump (Ctrl+Arrows),
        // selection (Shift+Arrows / Ctrl+A), and Backspace/Delete for the focused text box.
        // Sets outEnterPressedOnField4 = true if Enter was pressed while editing fieldId == 4.
        void UpdateFocusedTextField(Scene::Scene& scene,
                                    Scene::EntityID selectedEntityId,
                                    std::string& field4Buffer,
                                    bool isPlaying,
                                    bool ctrlDown,
                                    bool shiftDown,
                                    bool& outEnterPressedOnField4);

        // Hit-test helper for 2D UI rectangles in screen space.
        static bool IsMouseOver(float x, float y, float w, float h);

        // Immediate-mode UI widget primitives
        bool DrawButton(float x, float y, float w, float h, const char* label,
                        Renderer::ColorRGBA bgColor, Renderer::ColorRGBA hoverColor);
        bool DrawCheckbox(float x, float y, const char* label, bool& value);
        void DrawFloatSlider(float x, float y, float w, const char* label,
                             float& value, float minVal, float maxVal, int sliderId);
        void DrawIntSlider(float x, float y, float w, const char* label,
                           int& value, int minVal, int maxVal, int sliderId);
        void DrawColorPickerRow(float x, float y, float w, const char* label,
                                Renderer::ColorRGBA& color, int baseSliderId);
        void DrawTextureSelector(float x, float y, float w, Scene::SpriteComponent& sprite);
        void DrawSpriteRegionControls(float x, float y, float w, Scene::SpriteComponent& sprite, int baseSliderId);
        void DrawSpriteAnimatorControls(float x, float y, float w, Scene::SpriteComponent& sprite, int baseSliderId);
        float DrawColliderControls(float x, float y, float w, Scene::ColliderComponent& collider,
                                   const Scene::TransformComponent& transform, int baseSliderId);
        float DrawCameraControls(float x, float y, float w, Scene::CameraComponent& camera, int baseSliderId);
        float DrawAudioControls(float x, float y, float w, Scene::AudioComponent& audio, int baseSliderId);
        void DrawSortingControls(float x, float y, float w, int& sortingLayer, int& zOrder, int zSliderId);
        void DrawTextInputBox(float x, float y, float w, const char* label,
                              std::string& textBuffer, int fieldId);

        // Binds a non-entity string buffer (e.g. Project Dialog fields with fieldId >= 10)
        // so UpdateFocusedTextField can edit it with full caret/selection/word-jump support.
        void BindExternalTextField(int fieldId, std::string* buffer);

        // State accessors & mutators for Undo/Redo and modal coordination
        int GetFocusedTextFieldId() const { return m_focusedTextFieldId; }
        void SetFocusedTextFieldId(int id) { m_focusedTextFieldId = id; }
        void UnfocusTextField() {
            m_focusedTextFieldId = -1;
            m_isDraggingText = false;
        }
        void CancelSliderDrag() { m_activeSliderId = -1; }

        int& GetTextCursorPosRef() { return m_textCursorPos; }
        int& GetTextSelectionAnchorRef() { return m_textSelectionAnchor; }
        int GetTextCursorPos() const { return m_textCursorPos; }
        int GetTextSelectionAnchor() const { return m_textSelectionAnchor; }
        void ResetTextCaret() {
            m_textCursorPos = 0;
            m_textSelectionAnchor = 0;
        }

        void PushUndoIfActive();

    private:
        void BeginContinuousIfActive();

    private:
        static constexpr int MAX_EXTERNAL_TEXT_FIELDS = 8;
        struct ExternalTextBinding {
            int fieldId = -1;
            std::string* buffer = nullptr;
        };

        Scene::Scene* m_activeScene = nullptr;
        EditorUndoRedo* m_undoRedo = nullptr;
        Scene::EntityID m_selectedEntityId = 0;
        bool m_isPlaying = false;

        int m_activeSliderId = -1;
        int m_focusedTextFieldId = -1;
        int m_textCursorPos = 0;
        int m_textSelectionAnchor = 0;
        bool m_isDraggingText = false;
        ExternalTextBinding m_externalFields[MAX_EXTERNAL_TEXT_FIELDS] = {};
    };

}
