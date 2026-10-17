#pragma once
#include <vector>
#include <string>
#include "scene/scene.h"
#include "renderer/renderer2d.h"

namespace Talos::Editor {

    // =========================================================================
    // Editor History Snapshot (Undo / Redo State)
    // =========================================================================
    // Captures a complete, self-contained snapshot of all entities in the Scene
    // along with the currently selected EntityID. Because Talos Entities are flat
    // value structs without heap pointers or deep inheritance trees, storing and
    // restoring full snapshots is instantaneous and immune to dangling references.
    struct EditorHistoryState {
        std::vector<Scene::Entity> entities;
        Scene::EntityID selectedEntityId = 0;
        int textCursorPos = 0;
        int textSelectionAnchor = 0;
    };

    // =========================================================================
    // Visual Editor Subsystem (Layer 5: Editor Layer)
    // =========================================================================
    // Sits at the very top of the engine architecture. The Editor inspects and
    // mutates a Scene instance, handles viewport mouse picking/dragging, manages
    // Play/Stop simulation state snapshots, provides universal Undo/Redo (Ctrl+Z / Ctrl+Y),
    // and triggers Scene Save/Load operations.
    //
    // Architectural Note: Lower layers (Core, Renderer, Physics, Scene) have zero
    // awareness of this class. A standalone game build simply omits the Editor layer.
    class Editor {
    public:
        explicit Editor(int windowWidth, int windowHeight, const std::string& sceneFilePath = "examples/scenes/main.talos");

        // Processes keyboard shortcuts (Ctrl+Z / Ctrl+Y), Editor UI interactions,
        // viewport entity picking/dragging, and advances the Scene simulation in Play Mode.
        void Update(Scene::Scene& scene, float delta);

        // Renders the 2D viewport grid, the Scene entities, selection gizmos,
        // and all Editor panels (Toolbar, Entity List, and First-Level Inspector).
        void Render(Scene::Scene& scene);

        // Returns true when the game simulation is actively running (Play Mode).
        bool IsPlaying() const { return m_isPlaying; }

        // Selects an entity by ID so its properties appear in the Inspector.
        void SelectEntity(Scene::EntityID id) { m_selectedEntityId = id; }

        // Reverts the Scene and selection to the previous state on the Undo stack (Ctrl+Z).
        void Undo(Scene::Scene& scene);

        // Re-applies a previously undone state from the Redo stack (Ctrl+Y / Ctrl+Shift+Z).
        void Redo(Scene::Scene& scene);

    private:
        // State transition helpers for instant Play/Stop iteration without compilation
        void StartPlayMode(const Scene::Scene& scene);
        void StopPlayMode(Scene::Scene& scene);

        // Undo / Redo history management helpers
        void PushUndoState(const Scene::Scene& scene);
        void BeginContinuousEdit(const Scene::Scene& scene);
        void CommitContinuousEdit(const Scene::Scene& scene);

        // Displays a temporary notification message in the Top Toolbar (e.g., after Save/Load/Undo)
        void ShowStatusMessage(const std::string& message, float durationSeconds = 2.0f);

        // Viewport interaction (2D Camera Pan/Zoom, mouse picking & entity dragging in Edit Mode)
        void HandleViewportInput(Scene::Scene& scene);
        void ResetCamera();
        void FocusSelectedEntity(const Scene::Scene& scene);

        // Immediate-mode UI panel rendering helpers
        void RenderViewportGrid() const;
        void RenderSelectionGizmo(const Scene::Scene& scene) const;
        void RenderViewportCameraOverlay(const Scene::Scene& scene);
        void RenderViewportDiagnosticOverlay(const Scene::Scene& scene) const;
        void RenderConsolePanel();
        void RenderTopToolbar(Scene::Scene& scene);
        void RenderEntityListPanel(Scene::Scene& scene);
        void RenderInspectorPanel(Scene::Scene& scene);

        // Returns the active height in pixels occupied by the bottom Debug Console panel
        float GetConsolePanelHeight() const;

        // Lightweight immediate-mode UI widget primitives built strictly on Core::Input & Renderer2D
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
        void DrawTextInputBox(float x, float y, float w, const char* label,
                              std::string& textBuffer, int fieldId);

    private:
        int m_windowWidth = 1360;
        int m_windowHeight = 780;
        std::string m_sceneFilePath = "examples/scenes/main.talos";

        // Layout metrics (in screen pixels)
        static constexpr float TOOLBAR_HEIGHT = 46.0f;
        static constexpr float LEFT_PANEL_WIDTH = 230.0f;
        static constexpr float RIGHT_PANEL_WIDTH = 350.0f;
        static constexpr float CONSOLE_HEIGHT = 185.0f;
        static constexpr float CONSOLE_HEADER_HEIGHT = 28.0f;
        static constexpr size_t MAX_UNDO_STEPS = 128;

        // Play / Edit simulation state
        bool m_isPlaying = false;
        std::vector<Scene::Entity> m_sceneSnapshot; // Saved before Play, restored on Stop

        // Persistent Viewport Diagnostic Overlay state (toggled by F3 ONLY during Play Mode;
        // preserves its state across Stop -> Play transitions unless explicitly toggled off).
        bool m_diagnosticOverlayEnabled = false;
        float m_smoothedFrameDelta = 0.01667f;

        // Universal Undo / Redo History Stacks
        std::vector<EditorHistoryState> m_undoStack;
        std::vector<EditorHistoryState> m_redoStack;
        EditorHistoryState m_pendingSnapshot;
        bool m_hasPendingSnapshot = false;
        Scene::Scene* m_activeScene = nullptr; // Non-owning pointer set during Render/Update for UI widgets

        // Interactive 2D Viewport Camera (Pan & Zoom state)
        Renderer::Camera2DView m_camera;
        bool m_isPanningCamera = false;
        float m_panLastMouseX = 0.0f;
        float m_panLastMouseY = 0.0f;

        // Selection and Viewport dragging state
        Scene::EntityID m_selectedEntityId = 0;
        bool m_isDraggingEntity = false;
        float m_dragOffsetX = 0.0f;
        float m_dragOffsetY = 0.0f;

        // Immediate-mode UI interaction state (active slider or focused text input field)
        int m_activeSliderId = -1;
        int m_focusedTextFieldId = -1;
        int m_textCursorPos = 0;       // Current caret index inside the focused text buffer
        int m_textSelectionAnchor = 0; // Selection start index (differs from cursor when selecting)
        bool m_isDraggingText = false; // True while dragging the mouse across a text input box
        int m_newEntityCounter = 1;

        // In-Editor Debug Console state (severity filters, collapse toggle, and scroll offset)
        bool m_consoleOpen = true;
        bool m_showLogInfo = true;
        bool m_showLogWarn = true;
        bool m_showLogError = true;
        int m_consoleScrollOffset = 0;
        size_t m_lastLogCount = 0;

        // Temporary feedback notification state for Save/Load/Undo/Redo operations
        std::string m_statusMessage;
        float m_statusTimer = 0.0f;
    };

}
