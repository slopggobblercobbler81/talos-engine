// Bottom dock hosting the classified Debug Console and the Visual Asset Browser tabs
#pragma once
#include <cstddef>
#include <string>
#include "scene/scene.h"
#include "editor/gui/editor_gui.h"
#include "editor/history/editor_undo_redo.h"
#include "editor/docks/asset_browser_dock.h"

namespace Talos::Editor {

    // Active tab inside the bottom dock panel
    enum class BottomDockTab : int {
        DebugConsole = 0,
        AssetBrowser = 1
    };

    // =========================================================================
    // Bottom Console & Asset Browser Dock (Layer 5: Editor -> Docks Branch)
    // =========================================================================
    // Renders the bottom dock with a tab switcher between:
    //   1. DEBUG CONSOLE: Classified logs ([INFO], [WARN], [ERROR]), timestamps,
    //      [file:line] source locations, severity filters, scrolling, and Clear.
    //   2. ASSET BROWSER: Project directory navigation, category filtering,
    //      live texture/audio/scene previews, and one-click entity assignment.
    class ConsoleDock {
    public:
        // Returns the vertical height currently occupied by the bottom dock.
        float GetPanelHeight() const;

        BottomDockTab GetActiveTab() const { return m_activeTab; }
        void SetActiveTab(BottomDockTab tab) { m_activeTab = tab; }

        // Renders the bottom dock header bar (with tab switcher) and active tab body.
        void Render(int windowWidth,
                    int windowHeight,
                    Scene::Scene& scene,
                    Scene::EntityID& inOutSelectedEntityId,
                    std::string& inOutSceneFilePath,
                    bool isPlaying,
                    EditorGui& gui,
                    EditorUndoRedo& undoRedo,
                    AssetBrowserDock& assetBrowser,
                    std::string& outStatusMessage);

    private:
        BottomDockTab m_activeTab = BottomDockTab::DebugConsole;
        bool m_consoleOpen = true;
        bool m_showLogInfo = true;
        bool m_showLogWarn = true;
        bool m_showLogError = true;
        int m_consoleScrollOffset = 0;
        size_t m_lastLogCount = 0;
    };

}
