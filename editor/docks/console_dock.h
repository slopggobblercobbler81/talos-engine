// Bottom debug console dock with severity filters
#pragma once
#include <cstddef>
#include "editor/gui/editor_gui.h"

namespace Talos::Editor {

    // =========================================================================
    // Classified Debug Console Dock (Layer 5: Editor -> Docks Branch)
    // =========================================================================
    // Renders the bottom DEBUG CONSOLE dock displaying classified messages
    // ([INFO], [WARN], [ERROR]), timestamps, and exact [file:line] source
    // locations with severity filtering, scrolling, Clear, and Hide/Show.
    class ConsoleDock {
    public:
        // Returns the vertical height currently occupied by the console dock.
        float GetPanelHeight() const;

        // Renders the console header bar and scrollable message list.
        void Render(int windowWidth, int windowHeight, EditorGui& gui);

    private:
        bool m_consoleOpen = true;
        bool m_showLogInfo = true;
        bool m_showLogWarn = true;
        bool m_showLogError = true;
        int m_consoleScrollOffset = 0;
        size_t m_lastLogCount = 0;
    };

}
