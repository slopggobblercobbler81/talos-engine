#include "editor/docks/scene_tree_dock.h"
#include "core/input.h"
#include "core/logger.h"
#include "renderer/renderer2d.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Editor::EditorLayout;

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
                               Scene::EntityID& inOutSelectedEntityId,
                               bool isPlaying,
                               EditorGui& gui,
                               EditorUndoRedo& undoRedo,
                               std::string* outStatusMessage) {
        float h = static_cast<float>(windowHeight) - TOOLBAR_HEIGHT;

        // Left Panel Background
        Renderer2D::DrawRect(0.0f, TOOLBAR_HEIGHT, LEFT_PANEL_WIDTH, h, { 24, 25, 31, 255 });
        Renderer2D::DrawLine2D(LEFT_PANEL_WIDTH, TOOLBAR_HEIGHT, LEFT_PANEL_WIDTH, static_cast<float>(windowHeight), { 55, 58, 70, 255 });

        auto& allEntities = scene.GetEntities();
        std::vector<size_t> visibleIndices;
        visibleIndices.reserve(allEntities.size());
        for (size_t i = 0; i < allEntities.size(); ++i) {
            if (Scene::Scene::MatchesEntityFilter(allEntities[i], m_searchQuery, m_tagFilter)) {
                visibleIndices.push_back(i);
            }
        }

        // 1. Header Bar: Title + Count Badge + Reorder Up/Down Buttons
        float headerY = TOOLBAR_HEIGHT + 10.0f;
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
        bool canMoveUp = !isPlaying && selIndex > 0;
        bool canMoveDown = !isPlaying && selIndex >= 0 && static_cast<size_t>(selIndex + 1) < allEntities.size();

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
        float searchY = TOOLBAR_HEIGHT + 33.0f;
        gui.DrawTextInputBox(8.0f, searchY, LEFT_PANEL_WIDTH - 40.0f, "Find:", m_searchQuery, SEARCH_FIELD_ID);
        if (m_searchQuery.empty() && gui.GetFocusedTextFieldId() != SEARCH_FIELD_ID) {
            Renderer2D::DrawLabel("Name or #tag...", 70.0f, searchY + 5.0f, 13, { 95, 102, 120, 200 });
        }
        bool hasSearch = !m_searchQuery.empty();
        if (gui.DrawButton(LEFT_PANEL_WIDTH - 28.0f, searchY, 20.0f, 23.0f, "X",
                           hasSearch ? ColorRGBA{ 115, 45, 52, 255 } : ColorRGBA{ 34, 36, 45, 255 },
                           hasSearch ? ColorRGBA{ 155, 58, 66, 255 } : ColorRGBA{ 44, 47, 58, 255 }) && hasSearch) {
            m_searchQuery.clear();
            if (gui.GetFocusedTextFieldId() == SEARCH_FIELD_ID) {
                gui.ResetTextCaret();
            }
        }

        // 3. Quick Tag Filter Chips ("All", "Player", "Enemy", "Coin", "Wall")
        float tagRowY = TOOLBAR_HEIGHT + 60.0f;
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
            if (gui.DrawButton(chipX, tagRowY, chipW, 19.0f, chipLabel, chipBg, chipHv)) {
                if (isAll || m_tagFilter == chipLabel) {
                    m_tagFilter.clear();
                } else {
                    m_tagFilter = chipLabel;
                }
                m_scrollOffset = 0.0f;
            }
        }

        Renderer2D::DrawLine2D(8.0f, TOOLBAR_HEIGHT + 84.0f, LEFT_PANEL_WIDTH - 8.0f, TOOLBAR_HEIGHT + 84.0f, { 45, 48, 58, 255 });

        // 4. Scrollable Entity Hierarchy List
        float listTopY = TOOLBAR_HEIGHT + 88.0f;
        float listBottomY = static_cast<float>(windowHeight) - std::max(0.0f, bottomPanelHeight);
        float listViewH = std::max(40.0f, listBottomY - listTopY);
        constexpr float ROW_HEIGHT = 30.0f;
        constexpr float ROW_STRIDE = 34.0f;

        float totalContentH = static_cast<float>(visibleIndices.size()) * ROW_STRIDE + 8.0f;
        float maxScroll = std::max(0.0f, totalContentH - listViewH);
        if (EditorGui::IsMouseOver(0.0f, listTopY, LEFT_PANEL_WIDTH, listViewH)) {
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
            if (isFiltered && gui.DrawButton(16.0f, listTopY + 38.0f, LEFT_PANEL_WIDTH - 32.0f, 22.0f,
                                             "Clear Search / Filter",
                                             { 44, 62, 92, 255 }, { 58, 82, 122, 255 })) {
                ClearFilters();
            }
            Renderer2D::EndViewportScissor();
            return;
        }

        float rowY = listTopY + 4.0f - m_scrollOffset;
        m_dropTargetEntityIndex = -1;
        float dropLineY = -1.0f;

        for (size_t visPos = 0; visPos < visibleIndices.size(); ++visPos) {
            size_t entityIdx = visibleIndices[visPos];
            const auto& entity = allEntities[entityIdx];
            bool isSelected = (entity.id == inOutSelectedEntityId);

            bool rowVisible = (rowY + ROW_HEIGHT >= listTopY && rowY <= listBottomY);
            bool isHovered = rowVisible &&
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

                if (isSelected && !isPlaying) {
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
    }

}

