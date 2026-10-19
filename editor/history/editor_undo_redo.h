// Snapshot-based Undo/Redo history with continuous-drag coalescing
#pragma once
#include <vector>
#include "scene/scene.h"

namespace Talos::Editor {

    // =========================================================================
    // Editor History Snapshot (Undo / Redo State)
    // =========================================================================
    // Captures a complete, self-contained snapshot of all entities in the Scene
    // along with the currently selected EntityID and active text caret state.
    struct EditorHistoryState {
        std::vector<Scene::Entity> entities;
        Scene::EntityID selectedEntityId = 0;
        int textCursorPos = 0;
        int textSelectionAnchor = 0;
    };

    // =========================================================================
    // Universal Undo / Redo Manager (Layer 5: Editor -> History Branch)
    // =========================================================================
    // Manages discrete and continuous (drag) history snapshots for the Editor,
    // analogous to Godot's EditorUndoRedoManager.
    class EditorUndoRedo {
    public:
        static constexpr size_t MAX_UNDO_STEPS = 128;

        // Pushes a discrete state snapshot onto the Undo stack and clears Redo.
        void PushUndoState(const Scene::Scene& scene,
                           Scene::EntityID selectedEntityId,
                           int textCursorPos,
                           int textSelectionAnchor,
                           bool isPlaying);

        // Captures a pending snapshot at the start of a continuous slider or viewport drag.
        void BeginContinuousEdit(const Scene::Scene& scene,
                                 Scene::EntityID selectedEntityId,
                                 int textCursorPos,
                                 int textSelectionAnchor,
                                 bool isPlaying);

        // Commits the pending continuous edit snapshot if any entity property changed.
        void CommitContinuousEdit(const Scene::Scene& scene);

        // Reverts the Scene and selection to the previous state on the Undo stack (Ctrl+Z).
        // Returns true if a snapshot was restored, or false if the Undo stack was empty.
        bool Undo(Scene::Scene& scene,
                  Scene::EntityID& inOutSelectedEntityId,
                  int& inOutTextCursorPos,
                  int& inOutTextSelectionAnchor,
                  bool isPlaying);

        // Re-applies a previously undone state from the Redo stack (Ctrl+Y / Ctrl+Shift+Z).
        // Returns true if a snapshot was restored, or false if the Redo stack was empty.
        bool Redo(Scene::Scene& scene,
                  Scene::EntityID& inOutSelectedEntityId,
                  int& inOutTextCursorPos,
                  int& inOutTextSelectionAnchor,
                  bool isPlaying);

        bool HasPendingSnapshot() const { return m_hasPendingSnapshot; }

        // Clears all Undo and Redo snapshots (used when opening or creating a project).
        void Clear() {
            m_undoStack.clear();
            m_redoStack.clear();
            m_hasPendingSnapshot = false;
        }

        // Value comparison helpers used by Undo/Redo and Inspector widgets
        static bool AreColorsEqual(const Renderer::ColorRGBA& a, const Renderer::ColorRGBA& b);
        static bool AreEntityListsEqual(const std::vector<Scene::Entity>& a,
                                        const std::vector<Scene::Entity>& b);

    private:
        std::vector<EditorHistoryState> m_undoStack;
        std::vector<EditorHistoryState> m_redoStack;
        EditorHistoryState m_pendingSnapshot;
        bool m_hasPendingSnapshot = false;
    };

}
