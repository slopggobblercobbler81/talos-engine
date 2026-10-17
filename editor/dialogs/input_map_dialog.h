#pragma once
#include <string>
#include "editor/gui/editor_gui.h"

namespace Talos::Editor {

    // =========================================================================
    // Project Input Map Dialog (Layer 5: Editor -> Dialogs Branch)
    // =========================================================================
    // Renders the centered Godot-style INPUT MAP CONFIGURATION modal window,
    // handling action creation, interactive key listening, key unbinding,
    // action deletion, live [ON]/[OFF] testing, and .talos persistence.
    class InputMapDialog {
    public:
        // Loads the Input Map from disk on startup (or writes defaults if missing).
        void Initialize(const std::string& filePath = "examples/scenes/input_map.talos");

        bool IsOpen() const { return m_isOpen; }
        bool IsListeningForKey() const { return m_isOpen && !m_listeningActionName.empty(); }

        // Toggles or closes the modal window and cleans up active listening / focus state.
        void Toggle(EditorGui& gui);
        void Close(EditorGui& gui);

        // Polls physical keys when listening for a new key binding.
        // Returns true if input was consumed by key listening this frame.
        bool UpdateKeyListening(std::string& outStatusMessage);

        // Creates a new action when Enter is pressed inside the Action Name text box (fieldId == 4).
        void SubmitNewAction(EditorGui& gui, std::string& outStatusMessage);

        // Renders the modal dialog over the central viewport.
        void Render(int windowWidth,
                    int windowHeight,
                    float consoleHeight,
                    EditorGui& gui,
                    std::string& outStatusMessage);

        std::string& GetNewActionNameBuffer() { return m_newActionNameBuffer; }

    private:
        bool m_isOpen = false;
        std::string m_filePath = "examples/scenes/input_map.talos";
        std::string m_newActionNameBuffer;
        std::string m_listeningActionName;
        int m_scrollOffset = 0;
    };

}
