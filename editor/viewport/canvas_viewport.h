#pragma once
#include <string>
#include "scene/scene.h"
#include "renderer/renderer2d.h"
#include "editor/gui/editor_gui.h"
#include "editor/history/editor_undo_redo.h"

namespace Talos::Editor {

    // =========================================================================
    // 2D Canvas Viewport Controller (Layer 5: Editor -> Viewport Branch)
    // =========================================================================
    // Manages the central 2D Viewport: interactive camera Pan & Zoom-to-Cursor,
    // world-space entity picking and dragging, infinite reference grid,
    // collider & selection gizmos, and the bottom-left camera control HUD.
    class CanvasViewport {
    public:
        // Releases active entity drag or camera pan when mouse buttons are released.
        void OnMouseReleased(bool leftReleased, bool middleOrRightReleased);

        // Cancels an active entity drag (e.g., when entering Play Mode or Undoing mid-drag).
        void CancelEntityDrag() { m_isDraggingEntity = false; }

        // Handles mouse wheel zoom, middle/right/Space+left pan, and entity picking/dragging.
        void HandleInput(Scene::Scene& scene,
                         int windowWidth,
                         int windowHeight,
                         float consoleHeight,
                         Scene::EntityID& inOutSelectedEntityId,
                         EditorGui& gui,
                         EditorUndoRedo& undoRedo,
                         bool isPlaying);

        // Resets the 2D camera to origin (0,0) at 100% zoom and returns a status message.
        std::string ResetCamera();

        // Centers the 2D camera on the selected entity (or resets if none) and returns a status message.
        std::string FocusSelectedEntity(const Scene::Scene& scene,
                                        Scene::EntityID selectedEntityId,
                                        int windowWidth,
                                        int windowHeight,
                                        float consoleHeight);

        // Renders the 2D world inside the viewport scissor rect (Grid + Scene + Gizmos).
        void RenderWorld(Scene::Scene& scene,
                         int windowWidth,
                         int windowHeight,
                         float consoleHeight,
                         Scene::EntityID selectedEntityId,
                         bool isPlaying) const;

        // Renders the bottom-left floating Camera Zoom/Reset/Focus HUD pill in Edit Mode.
        void RenderCameraOverlay(const Scene::Scene& scene,
                                 int windowWidth,
                                 int windowHeight,
                                 float consoleHeight,
                                 Scene::EntityID selectedEntityId,
                                 EditorGui& gui,
                                 std::string& outStatusMessage);

        const Renderer::Camera2DView& GetCamera() const { return m_camera; }

    private:
        void RenderGrid(int windowWidth, int windowHeight, float consoleHeight) const;
        void RenderSelectionGizmo(const Scene::Scene& scene, Scene::EntityID selectedEntityId) const;

    private:
        Renderer::Camera2DView m_camera;
        bool m_isPanningCamera = false;
        float m_panLastMouseX = 0.0f;
        float m_panLastMouseY = 0.0f;

        bool m_isDraggingEntity = false;
        float m_dragOffsetX = 0.0f;
        float m_dragOffsetY = 0.0f;
    };

}
