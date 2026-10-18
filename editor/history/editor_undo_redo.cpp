#include "editor/history/editor_undo_redo.h"
#include "core/logger.h"
#include <cmath>

namespace Talos::Editor {

    using namespace Talos::Renderer;
    using namespace Talos::Scene;

    bool EditorUndoRedo::AreColorsEqual(const ColorRGBA& a, const ColorRGBA& b) {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    }

    bool EditorUndoRedo::AreEntityListsEqual(const std::vector<Entity>& a, const std::vector<Entity>& b) {
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
                !AreColorsEqual(ea.sprite.color, eb.sprite.color) ||
                ea.sprite.sortingLayer != eb.sprite.sortingLayer ||
                ea.sprite.zOrder != eb.sprite.zOrder ||
                std::fabs(ea.sprite.sourceX - eb.sprite.sourceX) > 0.01f ||
                std::fabs(ea.sprite.sourceY - eb.sprite.sourceY) > 0.01f ||
                std::fabs(ea.sprite.sourceW - eb.sprite.sourceW) > 0.01f ||
                std::fabs(ea.sprite.sourceH - eb.sprite.sourceH) > 0.01f ||
                ea.sprite.flipX != eb.sprite.flipX ||
                ea.sprite.flipY != eb.sprite.flipY) return false;
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

    void EditorUndoRedo::PushUndoState(const Scene::Scene& scene,
                                       Scene::EntityID selectedEntityId,
                                       int textCursorPos,
                                       int textSelectionAnchor,
                                       bool isPlaying) {
        if (isPlaying) return;

        // Avoid pushing duplicate consecutive states if nothing changed since the top of the stack
        if (!m_undoStack.empty() && AreEntityListsEqual(m_undoStack.back().entities, scene.GetEntities())) {
            return;
        }

        if (m_undoStack.size() >= MAX_UNDO_STEPS) {
            m_undoStack.erase(m_undoStack.begin());
        }

        m_undoStack.push_back({ scene.GetEntities(), selectedEntityId, textCursorPos, textSelectionAnchor });
        m_redoStack.clear();
    }

    void EditorUndoRedo::BeginContinuousEdit(const Scene::Scene& scene,
                                             Scene::EntityID selectedEntityId,
                                             int textCursorPos,
                                             int textSelectionAnchor,
                                             bool isPlaying) {
        if (isPlaying || m_hasPendingSnapshot) return;
        m_pendingSnapshot = { scene.GetEntities(), selectedEntityId, textCursorPos, textSelectionAnchor };
        m_hasPendingSnapshot = true;
    }

    void EditorUndoRedo::CommitContinuousEdit(const Scene::Scene& scene) {
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

    bool EditorUndoRedo::Undo(Scene::Scene& scene,
                              Scene::EntityID& inOutSelectedEntityId,
                              int& inOutTextCursorPos,
                              int& inOutTextSelectionAnchor,
                              bool isPlaying) {
        if (isPlaying) return false;

        if (m_hasPendingSnapshot) {
            CommitContinuousEdit(scene);
        }

        if (m_undoStack.empty()) {
            TALOS_LOG_WARN("Undo requested (Ctrl+Z), but the Undo history stack is empty");
            return false;
        }

        // Save current state to Redo stack before restoring the previous Undo snapshot
        m_redoStack.push_back({
            scene.GetEntities(),
            inOutSelectedEntityId,
            inOutTextCursorPos,
            inOutTextSelectionAnchor
        });

        EditorHistoryState restored = m_undoStack.back();
        m_undoStack.pop_back();

        scene.GetEntities()      = restored.entities;
        inOutSelectedEntityId    = restored.selectedEntityId;
        inOutTextCursorPos       = restored.textCursorPos;
        inOutTextSelectionAnchor = restored.textSelectionAnchor;

        TALOS_LOG_INFO("Undo applied (Ctrl+Z) — restored previous scene snapshot");
        return true;
    }

    bool EditorUndoRedo::Redo(Scene::Scene& scene,
                              Scene::EntityID& inOutSelectedEntityId,
                              int& inOutTextCursorPos,
                              int& inOutTextSelectionAnchor,
                              bool isPlaying) {
        if (isPlaying) return false;

        if (m_redoStack.empty()) {
            TALOS_LOG_WARN("Redo requested (Ctrl+Y), but the Redo history stack is empty");
            return false;
        }

        // Save current state back onto the Undo stack before applying Redo
        m_undoStack.push_back({
            scene.GetEntities(),
            inOutSelectedEntityId,
            inOutTextCursorPos,
            inOutTextSelectionAnchor
        });

        EditorHistoryState restored = m_redoStack.back();
        m_redoStack.pop_back();

        scene.GetEntities()      = restored.entities;
        inOutSelectedEntityId    = restored.selectedEntityId;
        inOutTextCursorPos       = restored.textCursorPos;
        inOutTextSelectionAnchor = restored.textSelectionAnchor;

        TALOS_LOG_INFO("Redo applied (Ctrl+Y) — re-applied scene snapshot");
        return true;
    }

}
