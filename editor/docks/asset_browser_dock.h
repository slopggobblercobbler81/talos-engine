// Bottom Asset Browser dock for exploring project folders, previewing files, and assigning resources
#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "scene/scene.h"
#include "editor/gui/editor_gui.h"
#include "editor/history/editor_undo_redo.h"

namespace Talos::Editor {

    // Category filter tabs inside the Asset Browser header bar
    enum class AssetCategoryFilter : int {
        All      = 0,
        Textures = 1,
        Audio    = 2,
        Scenes   = 3,
        Scripts  = 4
    };

    // Classification of a discovered project file or folder
    enum class AssetEntryKind : int {
        Directory = 0,
        Texture   = 1,
        Audio     = 2,
        Scene     = 3,
        Script    = 4,
        Other     = 5
    };

    // Metadata for a single entry displayed in the Asset Browser list
    struct AssetEntry {
        std::string name;
        std::string relativePath;
        AssetEntryKind kind = AssetEntryKind::Other;
        std::size_t fileSizeBytes = 0;
    };

    // =========================================================================
    // Visual Asset Browser Dock (Layer 5: Editor -> Docks Branch)
    // =========================================================================
    // Provides a docked resource explorer in the bottom panel to navigate
    // project directories, filter by asset type, preview textures/audio/scenes,
    // and assign resources to the selected entity with a single click.
    class AssetBrowserDock {
    public:
        AssetBrowserDock();

        // Rescans the filesystem for the active directory and category filter.
        void Refresh();

        // Changes the active directory and rescans entries.
        void SetCurrentDirectory(const std::string& dirPath);
        const std::string& GetCurrentDirectory() const { return m_currentDirectory; }

        // Selects an asset by relative path and loads any lightweight text preview.
        void SelectAsset(const std::string& relativePath);
        const std::string& GetSelectedAssetPath() const { return m_selectedAssetPath; }

        AssetCategoryFilter GetCategoryFilter() const { return m_categoryFilter; }
        void SetCategoryFilter(AssetCategoryFilter filter);

        const std::vector<AssetEntry>& GetEntries() const { return m_entries; }

        // Renders the Asset Browser filter buttons, Refresh button, and Hide/Show toggle in the bottom header bar.
        void RenderHeaderControls(float consoleX,
                                  float consoleY,
                                  float consoleW,
                                  float tabsEndX,
                                  EditorGui& gui,
                                  bool& inOutPanelOpen);

        // Renders the 3-column Asset Browser body (Folders | File List | Live Preview & One-Click Assignment).
        void RenderBody(float consoleX,
                        float bodyY,
                        float consoleW,
                        float bodyH,
                        Scene::Scene& scene,
                        Scene::EntityID& inOutSelectedEntityId,
                        std::string& inOutSceneFilePath,
                        bool isPlaying,
                        EditorGui& gui,
                        EditorUndoRedo& undoRedo,
                        std::string& outStatusMessage);

        // Classifies a file extension into an AssetEntryKind.
        static AssetEntryKind ClassifyPath(const std::string& filePath, bool isDirectory);

    private:
        void LoadTextPreview(const std::string& filePath);
        const AssetEntry* FindSelectedEntry() const;

    private:
        std::string m_currentDirectory = "examples";
        std::string m_selectedAssetPath;
        AssetEntry m_selectedEntryCache;
        AssetCategoryFilter m_categoryFilter = AssetCategoryFilter::All;
        std::vector<AssetEntry> m_entries;
        std::vector<std::string> m_textPreviewLines;
        int m_scrollOffset = 0;
        int m_spawnedSpriteCounter = 1;
        bool m_initialized = false;
    };

}
