// Modal Project Workflow dialog for creating, opening, and saving project.talos
#pragma once
#include <string>
#include "scene/project_serializer.h"
#include "scene/scene.h"
#include "editor/gui/editor_gui.h"
#include "editor/history/editor_undo_redo.h"

namespace Talos::Editor {

    // =========================================================================
    // Project Workflow & Configuration Modal Dialog (Layer 5: Editor -> Dialogs)
    // =========================================================================
    // Provides the unified Project Workflow UI for creating, opening, editing,
    // and saving 'project.talos' (general settings, window resolution, target
    // FPS, initial scene path, audio bus volumes, and input action bindings).
    class ProjectDialog {
    public:
        // Initializes the project configuration from 'projectFilePath' (or writes defaults).
        bool Initialize(const std::string& projectFilePath, std::string& inOutSceneFilePath);

        // Registers the dialog's editable string buffers with EditorGui.
        void BindTextFields(EditorGui& gui);

        bool IsOpen() const { return m_isOpen; }
        void Toggle(EditorGui& gui);
        void Close(EditorGui& gui);

        const std::string& GetProjectFilePath() const { return m_projectFilePath; }
        std::string& GetProjectFilePathBuffer() { return m_projectFilePath; }

        const Scene::ProjectConfig& GetConfig() const { return m_config; }
        Scene::ProjectConfig& GetConfigMutable() { return m_config; }

        // Workflow Actions: Create, Open, and Save unified 'project.talos'
        bool CreateNewProject(Scene::Scene& scene,
                              std::string& inOutSceneFilePath,
                              Scene::EntityID& inOutSelectedEntityId,
                              EditorUndoRedo& undoRedo,
                              std::string& outStatusMessage);

        bool OpenProject(Scene::Scene& scene,
                         std::string& inOutSceneFilePath,
                         Scene::EntityID& inOutSelectedEntityId,
                         EditorUndoRedo& undoRedo,
                         std::string& outStatusMessage);

        bool SaveProject(const Scene::Scene& scene,
                         const std::string& activeScenePath,
                         std::string& outStatusMessage);

        // Captures live AudioSystem and InputMap changes and persists them into 'project.talos'.
        void SyncSubsystemsToProjectFile();

        // Renders the modal dialog over the central viewport.
        void Render(Scene::Scene& scene,
                    int windowWidth,
                    int windowHeight,
                    float consoleHeight,
                    std::string& inOutSceneFilePath,
                    Scene::EntityID& inOutSelectedEntityId,
                    EditorGui& gui,
                    EditorUndoRedo& undoRedo,
                    bool& outRequestOpenInputMap,
                    bool& outRequestOpenAudioMixer,
                    std::string& outStatusMessage);

    private:
        bool m_isOpen = false;
        std::string m_projectFilePath = "examples/project.talos";
        Scene::ProjectConfig m_config;
    };

}
