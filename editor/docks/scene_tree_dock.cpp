#include "editor/docks/scene_tree_dock.h"
#include "core/input.h"
#include "renderer/renderer2d.h"
#include <cstdio>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Editor::EditorLayout;

    void SceneTreeDock::Render(Scene::Scene& scene,
                               int windowHeight,
                               Scene::EntityID& inOutSelectedEntityId,
                               EditorGui& gui) {
        float h = static_cast<float>(windowHeight) - TOOLBAR_HEIGHT;

        // Left Panel Background
        Renderer2D::DrawRect(0.0f, TOOLBAR_HEIGHT, LEFT_PANEL_WIDTH, h, { 24, 25, 31, 255 });
        Renderer2D::DrawLine2D(LEFT_PANEL_WIDTH, TOOLBAR_HEIGHT, LEFT_PANEL_WIDTH, static_cast<float>(windowHeight), { 55, 58, 70, 255 });

        Renderer2D::DrawLabel("SCENE ENTITIES", 14.0f, TOOLBAR_HEIGHT + 14.0f, 16, { 150, 155, 175, 255 });
        Renderer2D::DrawLine2D(12.0f, TOOLBAR_HEIGHT + 36.0f, LEFT_PANEL_WIDTH - 12.0f, TOOLBAR_HEIGHT + 36.0f, { 45, 48, 58, 255 });

        float rowY = TOOLBAR_HEIGHT + 46.0f;
        for (const auto& entity : scene.GetEntities()) {
            bool isSelected = (entity.id == inOutSelectedEntityId);
            bool isHovered = EditorGui::IsMouseOver(8.0f, rowY, LEFT_PANEL_WIDTH - 16.0f, 30.0f);

            ColorRGBA rowBg = isSelected ? ColorRGBA{ 52, 95, 160, 255 }
                            : (isHovered ? ColorRGBA{ 38, 41, 52, 255 } : ColorRGBA{ 29, 31, 38, 255 });

            Renderer2D::DrawRect(8.0f, rowY, LEFT_PANEL_WIDTH - 16.0f, 30.0f, rowBg);

            // Render a 16x16 texture thumbnail if the entity has a texture assigned;
            // otherwise render a 16x16 solid color indicator swatch.
            if (entity.sprite.enabled && !entity.sprite.texturePath.empty()) {
                Renderer2D::DrawSprite(
                    entity.sprite.texturePath.c_str(),
                    16.0f, rowY + 7.0f, 16.0f, 16.0f,
                    entity.sprite.color,
                    entity.sprite.sourceX,
                    entity.sprite.sourceY,
                    entity.sprite.sourceW,
                    entity.sprite.sourceH,
                    entity.sprite.flipX,
                    entity.sprite.flipY
                );
            } else {
                ColorRGBA swatch = entity.sprite.enabled ? entity.sprite.color : entity.text.color;
                Renderer2D::DrawRect(16.0f, rowY + 7.0f, 16.0f, 16.0f, swatch);
            }

            Renderer2D::DrawLabel(entity.name.c_str(), 40.0f, rowY + 7.0f, 16, { 235, 235, 240, 255 });

            // Render compact Camera + SortingLayer + Z-Order badge aligned to the right edge of the row
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
            int badgeW = Renderer2D::MeasureTextWidth(depthBadge, 13);
            ColorRGBA badgeColor = isSelected
                ? ColorRGBA{ 215, 235, 255, 255 }
                : (entity.camera.enabled ? ColorRGBA{ 110, 225, 215, 230 } : ColorRGBA{ 130, 142, 168, 220 });
            Renderer2D::DrawLabel(depthBadge, LEFT_PANEL_WIDTH - 16.0f - static_cast<float>(badgeW), rowY + 8.0f, 13, badgeColor);

            if (isHovered && Input::IsMouseButtonPressed(MouseButton::Left)) {
                inOutSelectedEntityId = entity.id;
                gui.UnfocusTextField();
            }

            rowY += 34.0f;
        }
    }

}
