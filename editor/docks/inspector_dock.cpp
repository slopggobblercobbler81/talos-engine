#include "editor/docks/inspector_dock.h"
#include "renderer/renderer2d.h"

namespace Talos::Editor {

    using namespace Talos::Renderer;
    using namespace Talos::Scene;
    using namespace Talos::Editor::EditorLayout;

    void InspectorDock::Render(Scene::Scene& scene,
                               int windowWidth,
                               int windowHeight,
                               Scene::EntityID selectedEntityId,
                               EditorGui& gui) {
        float panelX = static_cast<float>(windowWidth) - RIGHT_PANEL_WIDTH;
        float panelH = static_cast<float>(windowHeight) - TOOLBAR_HEIGHT;

        // Right Panel Background
        Renderer2D::DrawRect(panelX, TOOLBAR_HEIGHT, RIGHT_PANEL_WIDTH, panelH, { 24, 25, 31, 255 });
        Renderer2D::DrawLine2D(panelX, TOOLBAR_HEIGHT, panelX, static_cast<float>(windowHeight), { 55, 58, 70, 255 });

        Renderer2D::DrawLabel("FIRST-LEVEL INSPECTOR", panelX + 16.0f, TOOLBAR_HEIGHT + 14.0f, 16, { 150, 155, 175, 255 });
        Renderer2D::DrawLine2D(panelX + 12.0f, TOOLBAR_HEIGHT + 36.0f, panelX + RIGHT_PANEL_WIDTH - 12.0f, TOOLBAR_HEIGHT + 36.0f, { 45, 48, 58, 255 });

        Entity* selected = scene.FindEntityById(selectedEntityId);
        if (!selected) {
            Renderer2D::DrawLabel("Select any entity in the viewport", panelX + 20.0f, TOOLBAR_HEIGHT + 65.0f, 16, { 120, 125, 140, 255 });
            Renderer2D::DrawLabel("or from the left panel to edit its", panelX + 20.0f, TOOLBAR_HEIGHT + 88.0f, 16, { 120, 125, 140, 255 });
            Renderer2D::DrawLabel("properties directly.", panelX + 20.0f, TOOLBAR_HEIGHT + 111.0f, 16, { 120, 125, 140, 255 });
            return;
        }

        float x = panelX + 16.0f;
        float w = RIGHT_PANEL_WIDTH - 32.0f;
        float y = TOOLBAR_HEIGHT + 44.0f;

        // 1. Entity Identity & Master Active Toggle
        gui.DrawCheckbox(x, y, "Active", selected->active);
        y += 24.0f;
        gui.DrawTextInputBox(x, y, w, "Name:", selected->name, 1);
        y += 32.0f;

        // 2. Transform Component (Always accessible on first level)
        Renderer2D::DrawLabel("TRANSFORM 2D", x, y, 15, { 95, 195, 255, 255 });
        y += 18.0f;
        gui.DrawFloatSlider(x, y, w, "Pos X", selected->transform.x, 230.0f, 980.0f, 10);
        y += 22.0f;
        gui.DrawFloatSlider(x, y, w, "Pos Y", selected->transform.y, 50.0f, 740.0f, 11);
        y += 22.0f;
        gui.DrawFloatSlider(x, y, w, "Width", selected->transform.width, 16.0f, 500.0f, 12);
        y += 22.0f;
        gui.DrawFloatSlider(x, y, w, "Height", selected->transform.height, 16.0f, 500.0f, 13);
        y += 28.0f;

        // 3. Sprite Component (2D Texture / Solid Box + Color / Tint)
        gui.DrawCheckbox(x, y, "SPRITE (2D Image / Box)", selected->sprite.enabled);
        if (selected->sprite.enabled) {
            if (selected->sprite.texturePath.empty()) {
                Renderer2D::DrawLabel("[Solid Box]", x + w - 74.0f, y + 2.0f, 14, { 135, 140, 155, 255 });
            } else if (Renderer2D::HasTexture(selected->sprite.texturePath.c_str())) {
                Renderer2D::DrawLabel("[Texture OK]", x + w - 82.0f, y + 2.0f, 14, { 90, 225, 135, 255 });
            } else {
                Renderer2D::DrawLabel("[Missing!]", x + w - 68.0f, y + 2.0f, 14, { 245, 95, 95, 255 });
            }
        }
        y += 24.0f;

        if (selected->sprite.enabled) {
            gui.DrawTextureSelector(x, y, w, selected->sprite);
            y += 58.0f;

            const char* colorLabel = selected->sprite.texturePath.empty() ? "Box Color" : "Img Tint";
            gui.DrawColorPickerRow(x, y, w, colorLabel, selected->sprite.color, 20);
            y += 82.0f;
        }

        // 4. Text Component (First-Level Text, Font Size & Color)
        gui.DrawCheckbox(x, y, "TEXT LABEL (First-Level)", selected->text.enabled);
        y += 24.0f;
        if (selected->text.enabled) {
            gui.DrawTextInputBox(x, y, w, "Text:", selected->text.content, 2);
            y += 28.0f;
            gui.DrawIntSlider(x, y, w, "Font Size", selected->text.fontSize, 12, 56, 30);
            y += 22.0f;
            gui.DrawFloatSlider(x, y, w, "Offset Y", selected->text.offsetY, -80.0f, 80.0f, 31);
            y += 24.0f;
            gui.DrawColorPickerRow(x, y, w, "Font Color", selected->text.color, 40);
            y += 82.0f;
        }

        // 5. Collider 2D Component
        gui.DrawCheckbox(x, y, "COLLIDER 2D (Solid Box)", selected->collider.enabled);
        y += 24.0f;

        // 6. Movement 2D Component
        gui.DrawCheckbox(x, y, "MOVEMENT (WASD Control)", selected->movement.enabled);
        y += 22.0f;
        if (selected->movement.enabled) {
            gui.DrawCheckbox(x + 14.0f, y, "Player Controlled", selected->movement.playerControlled);
            y += 22.0f;
            gui.DrawFloatSlider(x, y, w, "Speed", selected->movement.speed, 50.0f, 700.0f, 50);
        }
    }

}
