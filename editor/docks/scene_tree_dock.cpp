#include "editor/docks/scene_tree_dock.h"
#include "scene/scene_serializer.h"
#include "core/input.h"
#include "core/logger.h"
#include "renderer/renderer2d.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Editor::EditorLayout;

    namespace {

        std::string ExtractSceneDisplayName(const std::string& pathStr) {
            if (pathStr.empty()) {
                return "unsaved.talsc";
            }
            std::filesystem::path p(pathStr);
            std::string filename = p.filename().string();
            return filename.empty() ? pathStr : filename;
        }

        std::string ExtractSceneDirectory(const std::string& pathStr) {
            std::filesystem::path p(pathStr);
            if (p.has_parent_path()) {
                return p.parent_path().generic_string();
            }
            return "examples/scenes";
        }

        void SwitchEditorOrRuntimeScene(Scene::Scene& scene,
                                        const std::string& targetPath,
                                        std::string& inOutSceneFilePath,
                                        Scene::EntityID& inOutSelectedEntityId,
                                        bool isPlaying,
                                        EditorGui& gui,
                                        EditorUndoRedo& undoRedo,
                                        std::string* outStatusMessage) {
            if (targetPath.empty()) return;

            if (isPlaying) {
                if (scene.ChangeScene(targetPath, false)) {
                    inOutSceneFilePath = scene.GetSceneFilePath();
                    inOutSelectedEntityId = scene.GetEntities().empty() ? 0 : scene.GetEntities().front().id;
                    if (outStatusMessage) {
                        *outStatusMessage = "Switched runtime scene -> '" + inOutSceneFilePath + "'";
                    }
                } else if (outStatusMessage) {
                    *outStatusMessage = "Failed to switch scene '" + targetPath + "'";
                }
            } else {
                if (Scene::SceneSerializer::Load(scene, targetPath)) {
                    inOutSceneFilePath = scene.GetSceneFilePath();
                    inOutSelectedEntityId = scene.GetEntities().empty() ? 0 : scene.GetEntities().front().id;
                    gui.CancelSliderDrag();
                    gui.UnfocusTextField();
                    undoRedo.Clear();
                    if (outStatusMessage) {
                        *outStatusMessage = "Opened scene '" + inOutSceneFilePath + "'";
                    }
                } else if (outStatusMessage) {
                    *outStatusMessage = "Failed to load scene '" + targetPath + "'";
                }
            }
        }

    } // anonymous namespace

    void SceneTreeDock::BindTextFields(EditorGui& gui) {
        gui.BindExternalTextField(SEARCH_FIELD_ID, &m_searchQuery);
    }

    void SceneTreeDock::ClearFilters() {
        m_searchQuery.clear();
        m_tagFilter.clear();
        m_scrollOffset = 0.0f;
    }

    void SceneTreeDock::Render(Scene::Scene& scene,
                               int windowHeight,
                               float bottomPanelHeight,
                               std::string& inOutSceneFilePath,
                               Scene::EntityID& inOutSelectedEntityId,
                               bool isPlaying,
                               EditorGui& gui,
                               EditorUndoRedo& undoRedo,
                               std::string* outStatusMessage) {
        float h = static_cast<float>(windowHeight) - TOOLBAR_HEIGHT;

        // Left Panel Background
        Renderer2D::DrawRect(0.0f, TOOLBAR_HEIGHT, LEFT_PANEL_WIDTH, h, { 24, 25, 31, 255 });
        Renderer2D::DrawLine2D(LEFT_PANEL_WIDTH, TOOLBAR_HEIGHT, LEFT_PANEL_WIDTH, static_cast<float>(windowHeight), { 55, 58, 70, 255 });

        // 0. Active Scene Switcher Bar ([<] [SCN: filename.talsc] [>] [+New / Rld])
        std::string sceneDir = ExtractSceneDirectory(inOutSceneFilePath);
        std::vector<std::string> discoveredScenes = Scene::SceneSerializer::DiscoverScenes(sceneDir);
        std::string normalizedCurrent = Scene::SceneSerializer::NormalizeScenePath(inOutSceneFilePath, sceneDir);
        if (!normalizedCurrent.empty() &&
            std::find(discoveredScenes.begin(), discoveredScenes.end(), normalizedCurrent) == discoveredScenes.end()) {
            discoveredScenes.push_back(normalizedCurrent);
            std::sort(discoveredScenes.begin(), discoveredScenes.end());
        }

        int currentSceneIdx = -1;
        for (size_t i = 0; i < discoveredScenes.size(); ++i) {
            if (discoveredScenes[i] == normalizedCurrent) {
                currentSceneIdx = static_cast<int>(i);
                break;
            }
        }

        float sceneBarY = TOOLBAR_HEIGHT + 6.0f;
        bool canCycleScenes = discoveredScenes.size() > 1;
        if (gui.DrawButton(8.0f, sceneBarY, 18.0f, 21.0f, "<",
                           canCycleScenes ? ColorRGBA{ 42, 46, 60, 255 } : ColorRGBA{ 30, 32, 40, 255 },
                           canCycleScenes ? ColorRGBA{ 62, 68, 88, 255 } : ColorRGBA{ 30, 32, 40, 255 }) && canCycleScenes) {
            int count = static_cast<int>(discoveredScenes.size());
            int prevIdx = (currentSceneIdx <= 0) ? (count - 1) : (currentSceneIdx - 1);
            m_sceneDropdownOpen = false;
            SwitchEditorOrRuntimeScene(scene, discoveredScenes[static_cast<size_t>(prevIdx)],
                                       inOutSceneFilePath, inOutSelectedEntityId, isPlaying, gui, undoRedo, outStatusMessage);
        }

        std::string sceneBtnLabel = "SCN: " + ExtractSceneDisplayName(inOutSceneFilePath);
        if (sceneBtnLabel.size() > 20) {
            sceneBtnLabel = sceneBtnLabel.substr(0, 18) + "..";
        }
        float selectorX = 28.0f;
        float selectorW = LEFT_PANEL_WIDTH - 92.0f;
        ColorRGBA selectorBg = m_sceneDropdownOpen ? ColorRGBA{ 86, 52, 132, 255 } : ColorRGBA{ 56, 38, 88, 255 };
        ColorRGBA selectorHv = ColorRGBA{ 104, 64, 158, 255 };
        if (gui.DrawButton(selectorX, sceneBarY, selectorW, 21.0f, sceneBtnLabel.c_str(), selectorBg, selectorHv)) {
            m_sceneDropdownOpen = !m_sceneDropdownOpen;
        }

        float nextSceneBtnX = selectorX + selectorW + 2.0f;
        if (gui.DrawButton(nextSceneBtnX, sceneBarY, 18.0f, 21.0f, ">",
                           canCycleScenes ? ColorRGBA{ 42, 46, 60, 255 } : ColorRGBA{ 30, 32, 40, 255 },
                           canCycleScenes ? ColorRGBA{ 62, 68, 88, 255 } : ColorRGBA{ 30, 32, 40, 255 }) && canCycleScenes) {
            int count = static_cast<int>(discoveredScenes.size());
            int nextIdx = (currentSceneIdx < 0 || currentSceneIdx + 1 >= count) ? 0 : (currentSceneIdx + 1);
            m_sceneDropdownOpen = false;
            SwitchEditorOrRuntimeScene(scene, discoveredScenes[static_cast<size_t>(nextIdx)],
                                       inOutSceneFilePath, inOutSelectedEntityId, isPlaying, gui, undoRedo, outStatusMessage);
        }

        float actionBtnX = nextSceneBtnX + 20.0f;
        float actionBtnW = LEFT_PANEL_WIDTH - actionBtnX - 8.0f;
        if (!isPlaying) {
            if (gui.DrawButton(actionBtnX, sceneBarY, actionBtnW, 21.0f, "+New",
                               { 38, 102, 78, 255 }, { 52, 132, 100, 255 })) {
                m_sceneDropdownOpen = false;
                if (!inOutSceneFilePath.empty()) {
                    Scene::SceneSerializer::Save(scene, inOutSceneFilePath);
                }
                std::string newScenePath = Scene::SceneSerializer::BuildUniqueScenePath("scene", sceneDir);
                scene.Clear();
                scene.SetSceneFilePath(newScenePath);

                Scene::Entity& cam = scene.CreateEntity("MainCamera", 580.0f, 320.0f);
                cam.tag = "Camera";
                cam.sprite.enabled = false;
                cam.camera.enabled = true;
                cam.camera.isPrimary = true;

                Scene::Entity& actor = scene.CreateEntity("Player", 580.0f, 320.0f);
                actor.tag = "Player";
                actor.transform.width = 48.0f;
                actor.transform.height = 48.0f;
                actor.sprite.enabled = true;
                actor.sprite.color = { 75, 200, 255, 255 };
                actor.movement.enabled = true;
                actor.movement.playerControlled = true;
                actor.collider.enabled = true;

                Scene::SceneSerializer::Save(scene, newScenePath);
                inOutSceneFilePath = newScenePath;
                inOutSelectedEntityId = actor.id;
                gui.CancelSliderDrag();
                gui.UnfocusTextField();
                undoRedo.Clear();
                if (outStatusMessage) {
                    *outStatusMessage = "Created new scene '" + newScenePath + "'";
                }
            }
        } else {
            if (gui.DrawButton(actionBtnX, sceneBarY, actionBtnW, 21.0f, "Rld",
                               { 118, 76, 34, 255 }, { 152, 98, 44, 255 })) {
                m_sceneDropdownOpen = false;
                if (scene.ReloadCurrentScene(false)) {
                    inOutSceneFilePath = scene.GetSceneFilePath();
                    inOutSelectedEntityId = scene.GetEntities().empty() ? 0 : scene.GetEntities().front().id;
                    if (outStatusMessage) {
                        *outStatusMessage = "Reloaded active scene '" + inOutSceneFilePath + "'";
                    }
                }
            }
        }

        Renderer2D::DrawLine2D(8.0f, TOOLBAR_HEIGHT + 31.0f, LEFT_PANEL_WIDTH - 8.0f, TOOLBAR_HEIGHT + 31.0f, { 45, 48, 58, 255 });

        auto& allEntities = scene.GetEntities();
        std::vector<size_t> visibleIndices;
        visibleIndices.reserve(allEntities.size());
        for (size_t i = 0; i < allEntities.size(); ++i) {
            if (Scene::Scene::MatchesEntityFilter(allEntities[i], m_searchQuery, m_tagFilter)) {
                visibleIndices.push_back(i);
            }
        }

        // 1. Header Bar: Title + Count Badge + Reorder Up/Down Buttons
        float headerY = TOOLBAR_HEIGHT + 36.0f;
        char headerTitle[48];
        bool isFiltered = !m_searchQuery.empty() || !m_tagFilter.empty();
        if (isFiltered) {
            std::snprintf(headerTitle, sizeof(headerTitle), "ENTITIES (%zu/%zu)",
                          visibleIndices.size(), allEntities.size());
        } else {
            std::snprintf(headerTitle, sizeof(headerTitle), "SCENE ENTITIES (%zu)",
                          allEntities.size());
        }
        Renderer2D::DrawLabel(headerTitle, 10.0f, headerY + 2.0f, 14, { 150, 155, 175, 255 });

        int selIndex = scene.GetEntityIndex(inOutSelectedEntityId);
        bool canMoveUp = !isPlaying && !m_sceneDropdownOpen && selIndex > 0;
        bool canMoveDown = !isPlaying && !m_sceneDropdownOpen && selIndex >= 0 && static_cast<size_t>(selIndex + 1) < allEntities.size();

        float upBtnX = LEFT_PANEL_WIDTH - 52.0f;
        float downBtnX = LEFT_PANEL_WIDTH - 28.0f;
        if (gui.DrawButton(upBtnX, headerY - 1.0f, 20.0f, 19.0f, "^",
                           canMoveUp ? ColorRGBA{ 42, 62, 92, 255 } : ColorRGBA{ 32, 34, 42, 255 },
                           canMoveUp ? ColorRGBA{ 58, 86, 128, 255 } : ColorRGBA{ 38, 40, 50, 255 }) && canMoveUp) {
            undoRedo.PushUndoState(scene, inOutSelectedEntityId,
                                   gui.GetTextCursorPos(), gui.GetTextSelectionAnchor(), isPlaying);
            if (scene.MoveEntityUp(inOutSelectedEntityId)) {
                const Scene::Entity* moved = scene.FindEntityById(inOutSelectedEntityId);
                if (outStatusMessage && moved) {
                    *outStatusMessage = "Moved '" + moved->name + "' Up in Hierarchy";
                }
            }
        }
        if (gui.DrawButton(downBtnX, headerY - 1.0f, 20.0f, 19.0f, "v",
                           canMoveDown ? ColorRGBA{ 42, 62, 92, 255 } : ColorRGBA{ 32, 34, 42, 255 },
                           canMoveDown ? ColorRGBA{ 58, 86, 128, 255 } : ColorRGBA{ 38, 40, 50, 255 }) && canMoveDown) {
            undoRedo.PushUndoState(scene, inOutSelectedEntityId,
                                   gui.GetTextCursorPos(), gui.GetTextSelectionAnchor(), isPlaying);
            if (scene.MoveEntityDown(inOutSelectedEntityId)) {
                const Scene::Entity* moved = scene.FindEntityById(inOutSelectedEntityId);
                if (outStatusMessage && moved) {
                    *outStatusMessage = "Moved '" + moved->name + "' Down in Hierarchy";
                }
            }
        }

        // 2. Search Input Box (filters by Entity Name or Tag) + Clear [X] Button
        float searchY = TOOLBAR_HEIGHT + 59.0f;
        if (!m_sceneDropdownOpen) {
            gui.DrawTextInputBox(8.0f, searchY, LEFT_PANEL_WIDTH - 40.0f, "Find:", m_searchQuery, SEARCH_FIELD_ID);
            if (m_searchQuery.empty() && gui.GetFocusedTextFieldId() != SEARCH_FIELD_ID) {
                Renderer2D::DrawLabel("Name or #tag...", 70.0f, searchY + 5.0f, 13, { 95, 102, 120, 200 });
            }
        }
        bool hasSearch = !m_searchQuery.empty() && !m_sceneDropdownOpen;
        if (gui.DrawButton(LEFT_PANEL_WIDTH - 28.0f, searchY, 20.0f, 23.0f, "X",
                           hasSearch ? ColorRGBA{ 115, 45, 52, 255 } : ColorRGBA{ 34, 36, 45, 255 },
                           hasSearch ? ColorRGBA{ 155, 58, 66, 255 } : ColorRGBA{ 44, 47, 58, 255 }) && hasSearch) {
            m_searchQuery.clear();
            if (gui.GetFocusedTextFieldId() == SEARCH_FIELD_ID) {
                gui.ResetTextCaret();
            }
        }

        // 3. Quick Tag Filter Chips ("All", "Player", "Enemy", "Coin", "Wall")
        float tagRowY = TOOLBAR_HEIGHT + 86.0f;
        static const char* TAG_CHIPS[5] = { "All", "Player", "Enemy", "Coin", "Wall" };
        float chipGap = 3.0f;
        float chipW = (LEFT_PANEL_WIDTH - 16.0f - chipGap * 4.0f) / 5.0f;
        for (int t = 0; t < 5; ++t) {
            const char* chipLabel = TAG_CHIPS[t];
            bool isAll = (t == 0);
            bool activeChip = isAll ? m_tagFilter.empty() : (m_tagFilter == chipLabel);
            ColorRGBA chipBg = activeChip ? ColorRGBA{ 46, 112, 88, 255 } : ColorRGBA{ 34, 37, 46, 255 };
            ColorRGBA chipHv = activeChip ? ColorRGBA{ 60, 138, 108, 255 } : ColorRGBA{ 48, 52, 65, 255 };
            float chipX = 8.0f + static_cast<float>(t) * (chipW + chipGap);
            if (gui.DrawButton(chipX, tagRowY, chipW, 19.0f, chipLabel, chipBg, chipHv) && !m_sceneDropdownOpen) {
                if (isAll || m_tagFilter == chipLabel) {
                    m_tagFilter.clear();
                } else {
                    m_tagFilter = chipLabel;
                }
                m_scrollOffset = 0.0f;
            }
        }

        Renderer2D::DrawLine2D(8.0f, TOOLBAR_HEIGHT + 110.0f, LEFT_PANEL_WIDTH - 8.0f, TOOLBAR_HEIGHT + 110.0f, { 45, 48, 58, 255 });

        // 4. Scrollable Entity Hierarchy List
        float listTopY = TOOLBAR_HEIGHT + 114.0f;
        float listBottomY = static_cast<float>(windowHeight) - std::max(0.0f, bottomPanelHeight);
        float listViewH = std::max(40.0f, listBottomY - listTopY);
        constexpr float ROW_HEIGHT = 30.0f;
        constexpr float ROW_STRIDE = 34.0f;

        float totalContentH = static_cast<float>(visibleIndices.size()) * ROW_STRIDE + 8.0f;
        float maxScroll = std::max(0.0f, totalContentH - listViewH);
        if (!m_sceneDropdownOpen && EditorGui::IsMouseOver(0.0f, listTopY, LEFT_PANEL_WIDTH, listViewH)) {
            float wheel = Input::GetMouseWheelMove();
            if (std::fabs(wheel) > 0.001f) {
                m_scrollOffset -= wheel * 28.0f;
            }
        }
        m_scrollOffset = std::max(0.0f, std::min(m_scrollOffset, maxScroll));

        Renderer2D::BeginViewportScissor(
            0,
            static_cast<int>(listTopY),
            static_cast<int>(LEFT_PANEL_WIDTH),
            static_cast<int>(listViewH)
        );

        if (visibleIndices.empty()) {
            Renderer2D::DrawLabel("No matching entities.", 16.0f, listTopY + 14.0f, 14, { 125, 130, 148, 255 });
            if (isFiltered && !m_sceneDropdownOpen &&
                gui.DrawButton(16.0f, listTopY + 38.0f, LEFT_PANEL_WIDTH - 32.0f, 22.0f,
                               "Clear Search / Filter",
                               { 44, 62, 92, 255 }, { 58, 82, 122, 255 })) {
                ClearFilters();
            }
        }

        float rowY = listTopY + 4.0f - m_scrollOffset;
        m_dropTargetEntityIndex = -1;
        float dropLineY = -1.0f;

        for (size_t visPos = 0; visPos < visibleIndices.size(); ++visPos) {
            size_t entityIdx = visibleIndices[visPos];
            const auto& entity = allEntities[entityIdx];
            bool isSelected = (entity.id == inOutSelectedEntityId);

            bool rowVisible = (rowY + ROW_HEIGHT >= listTopY && rowY <= listBottomY);
            bool isHovered = rowVisible && !m_sceneDropdownOpen &&
                             EditorGui::IsMouseOver(0.0f, listTopY, LEFT_PANEL_WIDTH, listViewH) &&
                             EditorGui::IsMouseOver(8.0f, rowY, LEFT_PANEL_WIDTH - 16.0f, ROW_HEIGHT);

            // Track drag-and-drop target index while dragging a row
            if (m_isDraggingRow && isHovered) {
                float mouseY = Input::GetMouseY();
                if (mouseY < rowY + ROW_HEIGHT * 0.5f) {
                    m_dropTargetEntityIndex = static_cast<int>(entityIdx);
                    dropLineY = rowY - 2.0f;
                } else {
                    m_dropTargetEntityIndex = static_cast<int>(entityIdx);
                    dropLineY = rowY + ROW_HEIGHT + 1.0f;
                }
            }

            if (rowVisible) {
                ColorRGBA rowBg = isSelected ? ColorRGBA{ 52, 95, 160, 255 }
                                : (isHovered ? ColorRGBA{ 38, 41, 52, 255 } : ColorRGBA{ 29, 31, 38, 255 });

                Renderer2D::DrawRect(8.0f, rowY, LEFT_PANEL_WIDTH - 16.0f, ROW_HEIGHT, rowBg);

                // Render a 16x16 texture thumbnail if the entity has a texture assigned;
                // otherwise render a 16x16 solid color indicator swatch.
                if (entity.sprite.enabled && !entity.sprite.texturePath.empty()) {
                    Renderer2D::DrawSprite(
                        entity.sprite.texturePath.c_str(),
                        14.0f, rowY + 7.0f, 16.0f, 16.0f,
                        entity.sprite.color,
                        entity.sprite.sourceX,
                        entity.sprite.sourceY,
                        entity.sprite.sourceW,
                        entity.sprite.sourceH,
                        entity.sprite.flipX,
                        entity.sprite.flipY
                    );
                } else if (entity.sprite.enabled || entity.text.enabled) {
                    ColorRGBA swatch = entity.sprite.enabled ? entity.sprite.color : entity.text.color;
                    Renderer2D::DrawRect(14.0f, rowY + 7.0f, 16.0f, 16.0f, swatch);
                } else {
                    Renderer2D::DrawRect(14.0f, rowY + 7.0f, 16.0f, 16.0f, { 32, 38, 50, 255 });
                    Renderer2D::DrawRectOutline(14.0f, rowY + 7.0f, 16.0f, 16.0f, 1.0f, { 95, 195, 255, 220 });
                }

                ColorRGBA nameColor = entity.active
                    ? ColorRGBA{ 235, 235, 240, 255 }
                    : ColorRGBA{ 135, 140, 155, 200 };
                Renderer2D::DrawLabel(entity.name.c_str(), 36.0f, rowY + 7.0f, 15, nameColor);

                // Right-side badges & inline reorder buttons on the selected row
                float rightEdgeX = LEFT_PANEL_WIDTH - 12.0f;
                bool clickedInlineReorder = false;

                if (isSelected && !isPlaying && !m_sceneDropdownOpen) {
                    bool rowCanUp = entityIdx > 0;
                    bool rowCanDown = (entityIdx + 1) < allEntities.size();
                    float inlineDownX = rightEdgeX - 17.0f;
                    float inlineUpX = inlineDownX - 19.0f;

                    if (gui.DrawButton(inlineUpX, rowY + 5.0f, 17.0f, 20.0f, "^",
                                       rowCanUp ? ColorRGBA{ 36, 68, 118, 255 } : ColorRGBA{ 38, 60, 95, 180 },
                                       rowCanUp ? ColorRGBA{ 56, 98, 165, 255 } : ColorRGBA{ 38, 60, 95, 180 })) {
                        clickedInlineReorder = true;
                        if (rowCanUp) {
                            undoRedo.PushUndoState(scene, inOutSelectedEntityId,
                                                   gui.GetTextCursorPos(), gui.GetTextSelectionAnchor(), isPlaying);
                            scene.MoveEntityUp(entity.id);
                        }
                        Renderer2D::EndViewportScissor();
                        return;
                    }
                    if (gui.DrawButton(inlineDownX, rowY + 5.0f, 17.0f, 20.0f, "v",
                                       rowCanDown ? ColorRGBA{ 36, 68, 118, 255 } : ColorRGBA{ 38, 60, 95, 180 },
                                       rowCanDown ? ColorRGBA{ 56, 98, 165, 255 } : ColorRGBA{ 38, 60, 95, 180 })) {
                        clickedInlineReorder = true;
                        if (rowCanDown) {
                            undoRedo.PushUndoState(scene, inOutSelectedEntityId,
                                                   gui.GetTextCursorPos(), gui.GetTextSelectionAnchor(), isPlaying);
                            scene.MoveEntityDown(entity.id);
                        }
                        Renderer2D::EndViewportScissor();
                        return;
                    }
                    rightEdgeX = inlineUpX - 5.0f;
                } else {
                    char depthBadge[40];
                    if (entity.camera.enabled) {
                        std::snprintf(depthBadge, sizeof(depthBadge), "CAM %s Z:%d",
                                      Scene::SortingLayer::GetShortName(entity.sprite.sortingLayer),
                                      entity.sprite.zOrder);
                    } else {
                        std::snprintf(depthBadge, sizeof(depthBadge), "%s Z:%d",
                                      Scene::SortingLayer::GetShortName(entity.sprite.sortingLayer),
                                      entity.sprite.zOrder);
                    }
                    int badgeW = Renderer2D::MeasureTextWidth(depthBadge, 12);
                    ColorRGBA badgeColor = isSelected
                        ? ColorRGBA{ 215, 235, 255, 255 }
                        : (entity.camera.enabled ? ColorRGBA{ 110, 225, 215, 230 } : ColorRGBA{ 130, 142, 168, 220 });
                    Renderer2D::DrawLabel(depthBadge, rightEdgeX - static_cast<float>(badgeW), rowY + 9.0f, 12, badgeColor);
                    rightEdgeX -= static_cast<float>(badgeW) + 6.0f;
                }

                // Tag badge when the entity carries a tag
                if (!entity.tag.empty()) {
                    std::string tagText = "#" + entity.tag;
                    int tagW = Renderer2D::MeasureTextWidth(tagText.c_str(), 12);
                    int nameW = Renderer2D::MeasureTextWidth(entity.name.c_str(), 15);
                    if (36.0f + static_cast<float>(nameW) + 8.0f < rightEdgeX - static_cast<float>(tagW)) {
                        ColorRGBA tagColor = isSelected
                            ? ColorRGBA{ 175, 255, 210, 255 }
                            : ColorRGBA{ 98, 205, 152, 225 };
                        Renderer2D::DrawLabel(tagText.c_str(), rightEdgeX - static_cast<float>(tagW), rowY + 9.0f, 12, tagColor);
                    }
                }

                if (!clickedInlineReorder && isHovered && Input::IsMouseButtonPressed(MouseButton::Left)) {
                    inOutSelectedEntityId = entity.id;
                    gui.UnfocusTextField();
                    if (!isPlaying) {
                        m_draggingEntityId = entity.id;
                        m_dragStartMouseY = Input::GetMouseY();
                        m_isDraggingRow = false;
                    }
                }
            }

            rowY += ROW_STRIDE;
        }

        // Handle active drag-and-drop row reordering
        if (m_draggingEntityId != 0) {
            if (Input::IsMouseButtonDown(MouseButton::Left) && !isPlaying) {
                if (!m_isDraggingRow && std::fabs(Input::GetMouseY() - m_dragStartMouseY) >= 8.0f) {
                    m_isDraggingRow = true;
                }
                if (m_isDraggingRow && dropLineY >= listTopY && dropLineY <= listBottomY) {
                    Renderer2D::DrawRect(8.0f, dropLineY, LEFT_PANEL_WIDTH - 16.0f, 2.0f, { 95, 215, 255, 255 });
                }
            } else {
                if (m_isDraggingRow && m_dropTargetEntityIndex >= 0) {
                    int fromIdx = scene.GetEntityIndex(m_draggingEntityId);
                    if (fromIdx >= 0 && fromIdx != m_dropTargetEntityIndex) {
                        undoRedo.PushUndoState(scene, inOutSelectedEntityId,
                                               gui.GetTextCursorPos(), gui.GetTextSelectionAnchor(), isPlaying);
                        if (scene.MoveEntityToIndex(m_draggingEntityId, static_cast<size_t>(m_dropTargetEntityIndex))) {
                            const Scene::Entity* moved = scene.FindEntityById(m_draggingEntityId);
                            if (outStatusMessage && moved) {
                                *outStatusMessage = "Reordered '" + moved->name + "' in Hierarchy";
                            }
                        }
                    }
                }
                m_draggingEntityId = 0;
                m_isDraggingRow = false;
                m_dropTargetEntityIndex = -1;
            }
        }

        Renderer2D::EndViewportScissor();

        // 5. Multi-Scene Dropdown Selector Overlay (when opened from the top scene bar)
        if (m_sceneDropdownOpen) {
            float dropX = 8.0f;
            float dropY = TOOLBAR_HEIGHT + 30.0f;
            float dropW = LEFT_PANEL_WIDTH - 16.0f;
            float itemH = 24.0f;
            float dropH = static_cast<float>(discoveredScenes.size()) * (itemH + 2.0f) + 28.0f;

            Renderer2D::DrawRect(dropX, dropY, dropW, dropH, { 18, 20, 26, 252 });
            Renderer2D::DrawRectOutline(dropX, dropY, dropW, dropH, 1.0f, { 112, 82, 168, 255 });
            Renderer2D::DrawLabel(isPlaying ? "SWITCH RUNTIME SCENE (.talsc)" : "OPEN SCENE IN EDITOR (.talsc)",
                                  dropX + 8.0f, dropY + 6.0f, 12, { 195, 165, 245, 255 });

            float itemY = dropY + 24.0f;
            std::string chosenScenePath;
            for (const auto& scPath : discoveredScenes) {
                bool isCurrent = (scPath == normalizedCurrent);
                std::string label = (isCurrent ? "* " : "  ") + ExtractSceneDisplayName(scPath);
                ColorRGBA itemBg = isCurrent ? ColorRGBA{ 72, 46, 114, 255 } : ColorRGBA{ 30, 33, 42, 255 };
                ColorRGBA itemHv = ColorRGBA{ 96, 62, 148, 255 };
                if (gui.DrawButton(dropX + 6.0f, itemY, dropW - 12.0f, itemH, label.c_str(), itemBg, itemHv)) {
                    chosenScenePath = scPath;
                }
                itemY += itemH + 2.0f;
            }

            if (!chosenScenePath.empty()) {
                m_sceneDropdownOpen = false;
                SwitchEditorOrRuntimeScene(scene, chosenScenePath,
                                           inOutSceneFilePath, inOutSelectedEntityId, isPlaying, gui, undoRedo, outStatusMessage);
            } else if (Input::IsMouseButtonPressed(MouseButton::Left) &&
                       !EditorGui::IsMouseOver(dropX, sceneBarY, dropW, (dropY - sceneBarY) + dropH)) {
                m_sceneDropdownOpen = false;
            }
        }
    }

}


