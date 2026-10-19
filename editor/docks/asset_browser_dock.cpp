// Bottom Asset Browser dock implementation for folder navigation, previews, and one-click assignment
#include "editor/docks/asset_browser_dock.h"
#include "core/input.h"
#include "core/logger.h"
#include "renderer/renderer2d.h"
#include "audio/audio_system.h"
#include "scene/scene_serializer.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Editor::EditorLayout;
    namespace fs = std::filesystem;

    AssetBrowserDock::AssetBrowserDock() = default;

    AssetEntryKind AssetBrowserDock::ClassifyPath(const std::string& filePath, bool isDirectory) {
        if (isDirectory) return AssetEntryKind::Directory;

        fs::path p(filePath);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (ext == ".png" || ext == ".bmp" || ext == ".jpg" || ext == ".jpeg") return AssetEntryKind::Texture;
        if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") return AssetEntryKind::Audio;
        if (ext == ".prefab") return AssetEntryKind::Prefab;
        if (ext == ".talos") {
            return (p.filename().string() == "project.talos") ? AssetEntryKind::Other : AssetEntryKind::Scene;
        }
        if (ext == ".lua") return AssetEntryKind::Script;
        return AssetEntryKind::Other;
    }

    void AssetBrowserDock::SetCurrentDirectory(const std::string& dirPath) {
        std::error_code ec;
        if (dirPath == "examples/prefabs" && !fs::exists(dirPath, ec)) {
            fs::create_directories(dirPath, ec);
        }
        m_currentDirectory = (fs::exists(dirPath, ec) && fs::is_directory(dirPath, ec))
            ? fs::path(dirPath).generic_string()
            : std::string("examples");
        m_scrollOffset = 0;
        Refresh();
    }

    void AssetBrowserDock::SetCategoryFilter(AssetCategoryFilter filter) {
        if (m_categoryFilter != filter) {
            m_categoryFilter = filter;
            m_scrollOffset = 0;
            Refresh();
        }
    }

    void AssetBrowserDock::Refresh() {
        m_initialized = true;
        m_entries.clear();

        std::error_code ec;
        if (!fs::exists(m_currentDirectory, ec) || !fs::is_directory(m_currentDirectory, ec)) {
            m_currentDirectory = (fs::exists("examples", ec) && fs::is_directory("examples", ec)) ? "examples" : ".";
        }

        for (const auto& dirEntry : fs::directory_iterator(m_currentDirectory, ec)) {
            if (ec) break;
            std::string name = dirEntry.path().filename().string();
            if (name.empty() || name[0] == '.') continue;

            bool isDir = dirEntry.is_directory(ec);
            std::string relPath = dirEntry.path().generic_string();
            AssetEntryKind kind = ClassifyPath(relPath, isDir);

            if (!isDir && m_categoryFilter != AssetCategoryFilter::All) {
                if (m_categoryFilter == AssetCategoryFilter::Textures && kind != AssetEntryKind::Texture) continue;
                if (m_categoryFilter == AssetCategoryFilter::Audio    && kind != AssetEntryKind::Audio)   continue;
                if (m_categoryFilter == AssetCategoryFilter::Scenes   && kind != AssetEntryKind::Scene)   continue;
                if (m_categoryFilter == AssetCategoryFilter::Prefabs  && kind != AssetEntryKind::Prefab)  continue;
                if (m_categoryFilter == AssetCategoryFilter::Scripts  && kind != AssetEntryKind::Script)  continue;
            }

            m_entries.push_back({ name, relPath, kind, isDir ? 0 : static_cast<std::size_t>(dirEntry.file_size(ec)) });
        }

        std::sort(m_entries.begin(), m_entries.end(), [](const AssetEntry& a, const AssetEntry& b) {
            bool aDir = (a.kind == AssetEntryKind::Directory);
            bool bDir = (b.kind == AssetEntryKind::Directory);
            return (aDir != bDir) ? (aDir > bDir) : (a.name < b.name);
        });

        for (const auto& entry : m_entries) {
            if (entry.relativePath == m_selectedAssetPath) {
                m_selectedEntryCache = entry;
                break;
            }
        }
    }

    void AssetBrowserDock::SelectAsset(const std::string& relativePath) {
        m_selectedAssetPath = relativePath;
        for (const auto& entry : m_entries) {
            if (entry.relativePath == relativePath) {
                m_selectedEntryCache = entry;
                break;
            }
        }
        if (m_selectedEntryCache.kind == AssetEntryKind::Scene ||
            m_selectedEntryCache.kind == AssetEntryKind::Prefab ||
            m_selectedEntryCache.kind == AssetEntryKind::Script ||
            m_selectedEntryCache.kind == AssetEntryKind::Other) {
            LoadTextPreview(relativePath);
        } else {
            m_textPreviewLines.clear();
        }
    }

    void AssetBrowserDock::LoadTextPreview(const std::string& filePath) {
        m_textPreviewLines.clear();
        std::ifstream in(filePath);
        std::string line;
        while (in.is_open() && m_textPreviewLines.size() < 6 && std::getline(in, line)) {
            if (line.size() > 38) line = line.substr(0, 35) + "...";
            m_textPreviewLines.push_back(line);
        }
    }

    const AssetEntry* AssetBrowserDock::FindSelectedEntry() const {
        if (m_selectedAssetPath.empty()) return nullptr;
        for (const auto& entry : m_entries) {
            if (entry.relativePath == m_selectedAssetPath) return &entry;
        }
        return (m_selectedEntryCache.relativePath == m_selectedAssetPath) ? &m_selectedEntryCache : nullptr;
    }

    void AssetBrowserDock::RenderHeaderControls(float consoleX, float consoleY, float consoleW,
                                                float tabsEndX, EditorGui& gui, bool& inOutPanelOpen) {
        struct FilterDef { const char* label; AssetCategoryFilter filter; float width; };
        const FilterDef filters[] = {
            { "All",      AssetCategoryFilter::All,      42.0f },
            { "Textures", AssetCategoryFilter::Textures, 68.0f },
            { "Audio",    AssetCategoryFilter::Audio,    52.0f },
            { "Scenes",   AssetCategoryFilter::Scenes,   56.0f },
            { "Prefabs",  AssetCategoryFilter::Prefabs,  60.0f },
            { "Scripts",  AssetCategoryFilter::Scripts,  58.0f }
        };

        float curX = tabsEndX + 10.0f;
        for (const auto& f : filters) {
            bool active = (m_categoryFilter == f.filter);
            if (gui.DrawButton(curX, consoleY + 3.0f, f.width, 22.0f, f.label,
                               active ? ColorRGBA{ 48, 92, 150, 255 } : ColorRGBA{ 32, 35, 44, 255 },
                               { 64, 115, 180, 255 })) {
                SetCategoryFilter(f.filter);
            }
            curX += f.width + 4.0f;
        }

        float rightBtnX = consoleX + consoleW - 142.0f;
        if (gui.DrawButton(rightBtnX, consoleY + 3.0f, 64.0f, 22.0f, "Refresh",
                           { 45, 48, 60, 255 }, { 65, 70, 88, 255 })) {
            Refresh();
        }
        if (gui.DrawButton(rightBtnX + 70.0f, consoleY + 3.0f, 62.0f, 22.0f,
                           inOutPanelOpen ? "Hide" : "Show",
                           { 45, 48, 60, 255 }, { 65, 70, 88, 255 })) {
            inOutPanelOpen = !inOutPanelOpen;
        }
    }

    void AssetBrowserDock::RenderBody(float consoleX, float bodyY, float consoleW, float bodyH,
                                      Scene::Scene& scene, Scene::EntityID& inOutSelectedEntityId,
                                      std::string& inOutSceneFilePath, bool isPlaying,
                                      EditorGui& gui, EditorUndoRedo& undoRedo,
                                      std::string& outStatusMessage) {
        if (!m_initialized) Refresh();

        constexpr float folderColW  = 176.0f;
        constexpr float previewColW = 280.0f;
        float listColX = consoleX + folderColW;
        float listColW = std::max(140.0f, consoleW - folderColW - previewColW);
        float prevColX = listColX + listColW;

        Renderer2D::DrawLine2D(listColX, bodyY, listColX, bodyY + bodyH, { 42, 45, 56, 255 });
        Renderer2D::DrawLine2D(prevColX, bodyY, prevColX, bodyY + bodyH, { 42, 45, 56, 255 });

        // Column 1: Quick Folders & Parent Navigation
        Renderer2D::DrawRect(consoleX, bodyY, folderColW - 1.0f, bodyH, { 20, 22, 28, 255 });
        std::string dirLabel = "DIR: " + m_currentDirectory;
        if (dirLabel.size() > 24) dirLabel = "DIR: ..." + dirLabel.substr(dirLabel.size() - 18);
        Renderer2D::DrawLabel(dirLabel.c_str(), consoleX + 8.0f, bodyY + 6.0f, 13, { 145, 185, 235, 255 });

        if (gui.DrawButton(consoleX + 8.0f, bodyY + 24.0f, folderColW - 16.0f, 21.0f, "[.. Up Folder]",
                           { 36, 40, 52, 255 }, { 54, 60, 78, 255 })) {
            fs::path p(m_currentDirectory);
            SetCurrentDirectory((p.has_parent_path() && p != fs::path("examples") && p != fs::path("."))
                                ? p.parent_path().generic_string() : "examples");
        }

        struct BookmarkFolder { const char* label; const char* path; };
        const BookmarkFolder bookmarks[] = {
            { "examples (Root)", "examples" },
            { "assets/textures", "examples/assets/textures" },
            { "assets/audio",    "examples/assets/audio" },
            { "scenes",          "examples/scenes" },
            { "prefabs",         "examples/prefabs" }
        };
        float bmY = bodyY + 50.0f;
        for (const auto& bm : bookmarks) {
            bool isCurrent = (m_currentDirectory == bm.path);
            if (gui.DrawButton(consoleX + 8.0f, bmY, folderColW - 16.0f, 21.0f, bm.label,
                               isCurrent ? ColorRGBA{ 44, 78, 128, 255 } : ColorRGBA{ 28, 31, 40, 255 },
                               { 58, 96, 152, 255 })) {
                SetCurrentDirectory(bm.path);
            }
            bmY += 23.0f;
        }

        // Column 2: Scrollable Directory & Asset List
        constexpr float rowHeight = 24.0f;
        int maxVisibleRows = std::max(1, static_cast<int>((bodyH - 6.0f) / rowHeight));
        int totalRows = static_cast<int>(m_entries.size());
        int maxScroll = std::max(0, totalRows - maxVisibleRows);

        if (EditorGui::IsMouseOver(listColX, bodyY, listColW, bodyH)) {
            float wheel = Input::GetMouseWheelMove();
            if (wheel > 0.01f)       m_scrollOffset = std::max(0, m_scrollOffset - 1);
            else if (wheel < -0.01f) m_scrollOffset = std::min(maxScroll, m_scrollOffset + 1);
        }
        m_scrollOffset = std::max(0, std::min(m_scrollOffset, maxScroll));

        Renderer2D::BeginViewportScissor(static_cast<int>(listColX), static_cast<int>(bodyY),
                                         static_cast<int>(listColW), static_cast<int>(bodyH));
        if (m_entries.empty()) {
            Renderer2D::DrawLabel("No matching assets in this folder.",
                                  listColX + 12.0f, bodyY + 12.0f, 14, { 115, 120, 136, 255 });
        } else {
            int endIdx = std::min(totalRows, m_scrollOffset + maxVisibleRows);
            float rowY = bodyY + 3.0f;
            std::string navigateToDir;

            for (int i = m_scrollOffset; i < endIdx; ++i) {
                const AssetEntry& entry = m_entries[i];
                bool isSelected = (entry.relativePath == m_selectedAssetPath);
                float rowW = listColW - 12.0f;
                bool hovered = EditorGui::IsMouseOver(listColX + 4.0f, rowY, rowW, rowHeight - 1.0f);

                ColorRGBA rowBg = isSelected ? ColorRGBA{ 42, 76, 126, 255 }
                                : (hovered ? ColorRGBA{ 32, 36, 46, 255 }
                                           : ((i % 2 == 0) ? ColorRGBA{ 22, 24, 30, 255 } : ColorRGBA{ 18, 19, 24, 255 }));
                Renderer2D::DrawRect(listColX + 4.0f, rowY, rowW, rowHeight - 1.0f, rowBg);

                if (hovered && Input::IsMouseButtonPressed(MouseButton::Left)) {
                    if (entry.kind == AssetEntryKind::Directory) navigateToDir = entry.relativePath;
                    else SelectAsset(entry.relativePath);
                }

                float itemX = listColX + 10.0f;
                const char* badge = "[FILE]";
                ColorRGBA badgeColor = { 150, 155, 170, 255 };
                if (entry.kind == AssetEntryKind::Directory) {
                    badge = "[DIR]"; badgeColor = { 245, 195, 70, 255 };
                } else if (entry.kind == AssetEntryKind::Texture) {
                    badge = "[TEX]"; badgeColor = { 90, 210, 140, 255 };
                    Renderer2D::DrawSprite(entry.relativePath.c_str(), itemX, rowY + 3.0f, 16.0f, 16.0f, { 255, 255, 255, 255 });
                    itemX += 20.0f;
                } else if (entry.kind == AssetEntryKind::Audio) {
                    badge = "[AUD]"; badgeColor = { 85, 195, 255, 255 };
                } else if (entry.kind == AssetEntryKind::Scene) {
                    badge = "[SCN]"; badgeColor = { 205, 140, 255, 255 };
                } else if (entry.kind == AssetEntryKind::Prefab) {
                    badge = "[PFB]"; badgeColor = { 105, 230, 195, 255 };
                } else if (entry.kind == AssetEntryKind::Script) {
                    badge = "[LUA]"; badgeColor = { 255, 150, 85, 255 };
                }

                Renderer2D::DrawLabel(badge, itemX, rowY + 4.0f, 13, badgeColor);
                Renderer2D::DrawLabel(entry.name.c_str(), itemX + 46.0f, rowY + 4.0f, 14, { 230, 234, 242, 255 });

                if (entry.kind != AssetEntryKind::Directory) {
                    char sizeBuf[32];
                    std::snprintf(sizeBuf, sizeof(sizeBuf), "%.1f KB", static_cast<float>(entry.fileSizeBytes) / 1024.0f);
                    int sw = Renderer2D::MeasureTextWidth(sizeBuf, 12);
                    Renderer2D::DrawLabel(sizeBuf, listColX + rowW - static_cast<float>(sw) - 4.0f,
                                          rowY + 5.0f, 12, { 130, 138, 156, 255 });
                }
                rowY += rowHeight;
            }

            if (totalRows > maxVisibleRows) {
                float trackX = listColX + listColW - 6.0f, trackY = bodyY + 2.0f, trackH = bodyH - 4.0f;
                Renderer2D::DrawRect(trackX, trackY, 4.0f, trackH, { 30, 32, 40, 255 });
                float thumbH = std::max(16.0f, trackH * (static_cast<float>(maxVisibleRows) / static_cast<float>(totalRows)));
                float thumbY = trackY + (trackH - thumbH) * (static_cast<float>(m_scrollOffset) / static_cast<float>(maxScroll));
                Renderer2D::DrawRect(trackX, thumbY, 4.0f, thumbH, { 95, 105, 130, 255 });
            }

            if (!navigateToDir.empty()) {
                Renderer2D::EndViewportScissor();
                SetCurrentDirectory(navigateToDir);
                return;
            }
        }
        Renderer2D::EndViewportScissor();

        // Column 3: Live Asset Preview & One-Click Assignment
        Renderer2D::DrawRect(prevColX + 1.0f, bodyY, previewColW - 1.0f, bodyH, { 20, 22, 28, 255 });
        Scene::Entity* selectedEntity = scene.FindEntityById(inOutSelectedEntityId);
        bool canAssign = (selectedEntity != nullptr && !isPlaying);

        const AssetEntry* sel = FindSelectedEntry();
        if (!sel) {
            Renderer2D::DrawLabel("ASSET PREVIEW", prevColX + 10.0f, bodyY + 8.0f, 13, { 145, 155, 175, 255 });
            Renderer2D::DrawLabel("Select a texture, audio, prefab,", prevColX + 10.0f, bodyY + 32.0f, 13, { 110, 116, 132, 255 });
            Renderer2D::DrawLabel("or scene file to preview & assign.", prevColX + 10.0f, bodyY + 50.0f, 13, { 110, 116, 132, 255 });
            if (canAssign) {
                std::string saveBtnLabel = "Save '" + selectedEntity->name + "' as Prefab";
                if (saveBtnLabel.size() > 30) {
                    saveBtnLabel = "Save Selected as Prefab";
                }
                if (gui.DrawButton(prevColX + 8.0f, bodyY + 86.0f, previewColW - 16.0f, 28.0f, saveBtnLabel.c_str(),
                                   { 36, 108, 92, 255 }, { 50, 138, 118, 255 })) {
                    std::string prefabPath = Scene::SceneSerializer::BuildPrefabPath(selectedEntity->name);
                    if (Scene::SceneSerializer::SavePrefab(*selectedEntity, prefabPath)) {
                        SetCurrentDirectory("examples/prefabs");
                        SelectAsset(prefabPath);
                        outStatusMessage = "Saved Prefab -> '" + prefabPath + "'";
                    } else {
                        outStatusMessage = "Failed to save Prefab '" + prefabPath + "'";
                    }
                }
            }
            return;
        }

        if (sel->kind == AssetEntryKind::Texture) {
            float thumbBoxX = prevColX + 8.0f, thumbBoxY = bodyY + 8.0f, thumbBoxS = 68.0f;
            Renderer2D::DrawRect(thumbBoxX, thumbBoxY, thumbBoxS, thumbBoxS, { 30, 33, 42, 255 });
            Renderer2D::DrawRectOutline(thumbBoxX, thumbBoxY, thumbBoxS, thumbBoxS, 1.0f, { 62, 68, 86, 255 });
            Renderer2D::DrawSprite(sel->relativePath.c_str(), thumbBoxX + 4.0f, thumbBoxY + 4.0f,
                                   thumbBoxS - 8.0f, thumbBoxS - 8.0f, { 255, 255, 255, 255 });

            int texW = 0, texH = 0;
            Renderer2D::GetTextureSize(sel->relativePath.c_str(), texW, texH);
            std::size_t vramBytes = Renderer2D::GetTextureVRAMBytes(sel->relativePath.c_str());

            float infoX = thumbBoxX + thumbBoxS + 8.0f;
            Renderer2D::DrawLabel(sel->name.c_str(), infoX, bodyY + 8.0f, 14, { 235, 240, 250, 255 });

            char dimBuf[64], vramBuf[64];
            std::snprintf(dimBuf, sizeof(dimBuf), "Size: %dx%d px", texW, texH);
            std::snprintf(vramBuf, sizeof(vramBuf), "VRAM: %.1f KB", static_cast<float>(vramBytes) / 1024.0f);
            Renderer2D::DrawLabel(dimBuf, infoX, bodyY + 28.0f, 13, { 160, 195, 235, 255 });
            Renderer2D::DrawLabel(vramBuf, infoX, bodyY + 46.0f, 13, { 145, 215, 165, 255 });

            float btnY = bodyY + 84.0f, btnW = (previewColW - 22.0f) * 0.5f;
            if (gui.DrawButton(prevColX + 8.0f, btnY, btnW, 26.0f, "Assign Sprite",
                               canAssign ? ColorRGBA{ 40, 108, 72, 255 } : ColorRGBA{ 34, 36, 44, 255 },
                               canAssign ? ColorRGBA{ 54, 136, 92, 255 } : ColorRGBA{ 34, 36, 44, 255 }) && canAssign) {
                undoRedo.PushUndoState(scene, inOutSelectedEntityId,
                                       gui.GetTextCursorPos(), gui.GetTextSelectionAnchor(), isPlaying);
                selectedEntity->sprite.enabled = true;
                selectedEntity->sprite.texturePath = sel->relativePath;
                selectedEntity->sprite.sourceX = 0.0f;
                selectedEntity->sprite.sourceY = 0.0f;
                selectedEntity->sprite.sourceW = 0.0f;
                selectedEntity->sprite.sourceH = 0.0f;
                selectedEntity->sprite.frameCount = 1;
                selectedEntity->sprite.currentFrame = 0;
                outStatusMessage = "Assigned texture '" + sel->name + "' to '" + selectedEntity->name + "'";
                TALOS_LOG_INFO(outStatusMessage);
            }

            if (gui.DrawButton(prevColX + 14.0f + btnW, btnY, btnW, 26.0f, "+ New Sprite",
                               !isPlaying ? ColorRGBA{ 46, 86, 142, 255 } : ColorRGBA{ 34, 36, 44, 255 },
                               !isPlaying ? ColorRGBA{ 62, 112, 180, 255 } : ColorRGBA{ 34, 36, 44, 255 }) && !isPlaying) {
                undoRedo.PushUndoState(scene, inOutSelectedEntityId,
                                       gui.GetTextCursorPos(), gui.GetTextSelectionAnchor(), isPlaying);
                std::string entName = fs::path(sel->name).stem().string() + "_" + std::to_string(m_spawnedSpriteCounter++);
                Scene::Entity& created = scene.CreateEntity(entName);
                created.transform.width  = (texW > 0) ? static_cast<float>(texW) : 64.0f;
                created.transform.height = (texH > 0) ? static_cast<float>(texH) : 64.0f;
                created.sprite.enabled = true;
                created.sprite.texturePath = sel->relativePath;
                inOutSelectedEntityId = created.id;
                outStatusMessage = "Created sprite entity '" + entName + "' from '" + sel->name + "'";
                TALOS_LOG_INFO(outStatusMessage);
            }

            Renderer2D::DrawLabel(selectedEntity ? ("Target: " + selectedEntity->name).c_str()
                                                 : "Target: (Select an entity in Scene Tree)",
                                  prevColX + 8.0f, bodyY + 118.0f, 12, { 130, 138, 156, 255 });
        } else if (sel->kind == AssetEntryKind::Audio) {
            Audio::SoundInfo info = Audio::AudioSystem::GetSoundInfo(sel->relativePath);
            const char* fmtStr = (info.format == Audio::AudioFormat::WAV) ? "WAV"
                               : (info.format == Audio::AudioFormat::OGG) ? "OGG"
                               : (info.format == Audio::AudioFormat::MP3) ? "MP3" : "AUDIO";
            Renderer2D::DrawLabel(sel->name.c_str(), prevColX + 8.0f, bodyY + 8.0f, 14, { 235, 240, 250, 255 });

            char metaBuf[96];
            std::snprintf(metaBuf, sizeof(metaBuf), "%s | %u Hz | %u ch | %.2fs",
                          fmtStr, info.sampleRate, info.channels, info.durationSeconds);
            Renderer2D::DrawLabel(metaBuf, prevColX + 8.0f, bodyY + 28.0f, 12, { 145, 205, 245, 255 });

            float thirdW = (previewColW - 24.0f) / 3.0f, playY = bodyY + 48.0f;
            if (gui.DrawButton(prevColX + 8.0f, playY, thirdW, 24.0f, "Play SFX",
                               { 38, 86, 136, 255 }, { 54, 112, 172, 255 })) {
                Audio::AudioSystem::PlaySound(sel->relativePath, 1.0f, 1.0f);
            }
            if (gui.DrawButton(prevColX + 12.0f + thirdW, playY, thirdW, 24.0f, "Play Music",
                               { 46, 78, 128, 255 }, { 64, 102, 164, 255 })) {
                Audio::AudioSystem::PlayMusic(sel->relativePath, false, 1.0f, 1.0f);
            }
            if (gui.DrawButton(prevColX + 16.0f + thirdW * 2.0f, playY, thirdW, 24.0f, "Stop",
                               { 118, 44, 48, 255 }, { 150, 58, 64, 255 })) {
                Audio::AudioSystem::StopAllSounds();
                Audio::AudioSystem::StopAllMusic();
            }

            float assignY = bodyY + 80.0f, halfW = (previewColW - 22.0f) * 0.5f;
            if (gui.DrawButton(prevColX + 8.0f, assignY, halfW, 26.0f, "Assign as SFX",
                               canAssign ? ColorRGBA{ 40, 108, 72, 255 } : ColorRGBA{ 34, 36, 44, 255 },
                               canAssign ? ColorRGBA{ 54, 136, 92, 255 } : ColorRGBA{ 34, 36, 44, 255 }) && canAssign) {
                undoRedo.PushUndoState(scene, inOutSelectedEntityId,
                                       gui.GetTextCursorPos(), gui.GetTextSelectionAnchor(), isPlaying);
                selectedEntity->audio.enabled = true;
                selectedEntity->audio.audioPath = sel->relativePath;
                selectedEntity->audio.isMusic = false;
                outStatusMessage = "Assigned SFX '" + sel->name + "' to '" + selectedEntity->name + "'";
                TALOS_LOG_INFO(outStatusMessage);
            }
            if (gui.DrawButton(prevColX + 14.0f + halfW, assignY, halfW, 26.0f, "Assign as Music",
                               canAssign ? ColorRGBA{ 64, 88, 146, 255 } : ColorRGBA{ 34, 36, 44, 255 },
                               canAssign ? ColorRGBA{ 82, 112, 182, 255 } : ColorRGBA{ 34, 36, 44, 255 }) && canAssign) {
                undoRedo.PushUndoState(scene, inOutSelectedEntityId,
                                       gui.GetTextCursorPos(), gui.GetTextSelectionAnchor(), isPlaying);
                selectedEntity->audio.enabled = true;
                selectedEntity->audio.audioPath = sel->relativePath;
                selectedEntity->audio.isMusic = true;
                outStatusMessage = "Assigned Music '" + sel->name + "' to '" + selectedEntity->name + "'";
                TALOS_LOG_INFO(outStatusMessage);
            }

            Renderer2D::DrawLabel(selectedEntity ? ("Target: " + selectedEntity->name).c_str()
                                                 : "Target: (Select an entity in Scene Tree)",
                                  prevColX + 8.0f, bodyY + 114.0f, 12, { 130, 138, 156, 255 });
        } else if (sel->kind == AssetEntryKind::Scene) {
            Renderer2D::DrawLabel(sel->name.c_str(), prevColX + 8.0f, bodyY + 8.0f, 14, { 225, 195, 255, 255 });
            float lineY = bodyY + 28.0f;
            for (const auto& line : m_textPreviewLines) {
                Renderer2D::DrawLabel(line.c_str(), prevColX + 8.0f, lineY, 12, { 145, 152, 170, 255 });
                lineY += 14.0f;
                if (lineY > bodyY + 92.0f) break;
            }
            if (gui.DrawButton(prevColX + 8.0f, bodyY + 104.0f, previewColW - 16.0f, 28.0f, "Open Scene in Editor",
                               !isPlaying ? ColorRGBA{ 92, 56, 138, 255 } : ColorRGBA{ 34, 36, 44, 255 },
                               !isPlaying ? ColorRGBA{ 118, 74, 174, 255 } : ColorRGBA{ 34, 36, 44, 255 }) && !isPlaying) {
                if (Scene::SceneSerializer::Load(scene, sel->relativePath)) {
                    inOutSceneFilePath = sel->relativePath;
                    inOutSelectedEntityId = 0;
                    undoRedo.Clear();
                    outStatusMessage = "Opened scene '" + sel->relativePath + "'";
                } else {
                    outStatusMessage = "Failed to load scene '" + sel->relativePath + "'";
                }
            }
        } else if (sel->kind == AssetEntryKind::Prefab) {
            Renderer2D::DrawLabel(sel->name.c_str(), prevColX + 8.0f, bodyY + 8.0f, 14, { 105, 230, 195, 255 });
            float lineY = bodyY + 28.0f;
            for (const auto& line : m_textPreviewLines) {
                Renderer2D::DrawLabel(line.c_str(), prevColX + 8.0f, lineY, 12, { 145, 152, 170, 255 });
                lineY += 14.0f;
                if (lineY > bodyY + 92.0f) break;
            }
            if (gui.DrawButton(prevColX + 8.0f, bodyY + 104.0f, previewColW - 16.0f, 28.0f, "+ Instantiate in Scene",
                               !isPlaying ? ColorRGBA{ 36, 118, 96, 255 } : ColorRGBA{ 34, 36, 44, 255 },
                               !isPlaying ? ColorRGBA{ 50, 150, 122, 255 } : ColorRGBA{ 34, 36, 44, 255 }) && !isPlaying) {
                undoRedo.PushUndoState(scene, inOutSelectedEntityId,
                                       gui.GetTextCursorPos(), gui.GetTextSelectionAnchor(), isPlaying);
                Scene::Entity* inst = Scene::SceneSerializer::InstantiatePrefab(scene, sel->relativePath);
                if (inst) {
                    // Nudge position slightly if another entity already sits at the exact same coordinates
                    for (int guard = 0; guard < 64; ++guard) {
                        bool overlapped = false;
                        for (const auto& existing : scene.GetEntities()) {
                            if (existing.id == inst->id) continue;
                            float dx = existing.transform.x - inst->transform.x;
                            float dy = existing.transform.y - inst->transform.y;
                            if (dx > -0.5f && dx < 0.5f && dy > -0.5f && dy < 0.5f) {
                                overlapped = true;
                                break;
                            }
                        }
                        if (!overlapped) break;
                        inst->transform.x += 16.0f;
                        inst->transform.y += 16.0f;
                    }
                    inOutSelectedEntityId = inst->id;
                    outStatusMessage = "Instantiated Prefab '" + sel->name + "' -> '" + inst->name + "'";
                } else {
                    outStatusMessage = "Failed to instantiate Prefab '" + sel->relativePath + "'";
                }
            }
        } else {
            Renderer2D::DrawLabel(sel->name.c_str(), prevColX + 8.0f, bodyY + 8.0f, 14, { 235, 240, 250, 255 });
            float lineY = bodyY + 28.0f;
            for (const auto& line : m_textPreviewLines) {
                Renderer2D::DrawLabel(line.c_str(), prevColX + 8.0f, lineY, 12, { 145, 152, 170, 255 });
                lineY += 15.0f;
            }
        }
    }

}
