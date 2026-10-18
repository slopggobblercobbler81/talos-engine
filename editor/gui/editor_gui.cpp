#include "editor/gui/editor_gui.h"
#include "core/input.h"

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

}
