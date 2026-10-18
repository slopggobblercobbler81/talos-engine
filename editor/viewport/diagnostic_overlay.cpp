#include "editor/viewport/diagnostic_overlay.h"
#include "editor/gui/editor_gui.h"
#include "renderer/renderer2d.h"
#include "core/logger.h"
#include <algorithm>
#include <cstdio>

namespace Talos::Editor {

    using namespace Talos::Renderer;
    using namespace Talos::Scene;
    using namespace Talos::Editor::EditorLayout;

    namespace {

        void FormatBytes(size_t bytes, char* outBuf, size_t bufSize) {
            if (bytes < 1024) {
                std::snprintf(outBuf, bufSize, "%zu B", bytes);
            } else if (bytes < 1024 * 1024) {
                std::snprintf(outBuf, bufSize, "%.1f KB", static_cast<double>(bytes) / 1024.0);
            } else {
                std::snprintf(outBuf, bufSize, "%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
            }
        }

    } // anonymous namespace

    void DiagnosticOverlay::UpdateFrameDelta(float delta) {
        if (delta > 0.00001f && delta < 1.0f) {
            m_smoothedFrameDelta = m_smoothedFrameDelta * 0.90f + delta * 0.10f;
        }
    }

    bool DiagnosticOverlay::Toggle() {
        m_enabled = !m_enabled;
        TALOS_LOG_INFO(std::string("Viewport Diagnostic Overlay (F3) ") +
                       (m_enabled ? "ENABLED" : "DISABLED"));
        return m_enabled;
    }

    void DiagnosticOverlay::Render(const Scene::Scene& scene,
                                   int windowWidth,
                                   int windowHeight,
                                   float consoleHeight,
                                   Scene::EntityID selectedEntityId) const {
        const SceneDiagnostics& diag = scene.GetDiagnostics();

        int vpX = static_cast<int>(LEFT_PANEL_WIDTH);
        int vpY = static_cast<int>(TOOLBAR_HEIGHT);
        int vpW = windowWidth - static_cast<int>(LEFT_PANEL_WIDTH + RIGHT_PANEL_WIDTH);
        int vpH = windowHeight - static_cast<int>(TOOLBAR_HEIGHT + consoleHeight);
        if (vpW <= 40 || vpH <= 40) return;

        Renderer2D::BeginViewportScissor(vpX, vpY, vpW, vpH);

        float boxX = LEFT_PANEL_WIDTH + 10.0f;
        float boxY = TOOLBAR_HEIGHT + 10.0f;
        float boxW = std::min(680.0f, static_cast<float>(vpW) - 20.0f);
        float boxH = 266.0f;

        Renderer2D::DrawRect(boxX, boxY, boxW, boxH, { 14, 16, 22, 232 });
        Renderer2D::DrawRectOutline(boxX, boxY, boxW, boxH, 1.0f, { 70, 130, 180, 220 });

        // 1. Header Bar
        Renderer2D::DrawRect(boxX + 1.0f, boxY + 1.0f, boxW - 2.0f, 26.0f, { 22, 28, 38, 245 });
        Renderer2D::DrawLine2D(boxX, boxY + 27.0f, boxX + boxW, boxY + 27.0f, { 55, 85, 120, 220 });

        Renderer2D::DrawLabel(
            "VIEWPORT DIAGNOSTICS [F3]  -  COMPONENT CONSUMPTION",
            boxX + 10.0f, boxY + 6.0f, 14, { 95, 205, 255, 255 }
        );

        float fps = (m_smoothedFrameDelta > 0.0001f) ? (1.0f / m_smoothedFrameDelta) : 0.0f;
        float frameMs = m_smoothedFrameDelta * 1000.0f;
        char headerRight[96];
        std::snprintf(
            headerRight, sizeof(headerRight),
            "FPS: %.0f (%.2f ms)  |  Entities: %zu/%zu",
            fps, frameMs, diag.activeEntities, diag.totalEntities
        );
        int hrWidth = Renderer2D::MeasureTextWidth(headerRight, 14);
        Renderer2D::DrawLabel(
            headerRight,
            boxX + boxW - static_cast<float>(hrWidth) - 10.0f,
            boxY + 6.0f, 14, { 180, 235, 195, 255 }
        );

        // 2. Column Headers
        float colHeaderY = boxY + 32.0f;
        Renderer2D::DrawRect(boxX + 1.0f, colHeaderY - 3.0f, boxW - 2.0f, 20.0f, { 18, 21, 28, 240 });

        float colCompX  = boxX + 10.0f;
        float colInstX  = boxX + 122.0f;
        float colCpuX   = boxX + 196.0f;
        float colShareX = boxX + 318.0f;
        float colRamX   = boxX + 418.0f;
        float colVramX  = boxX + 534.0f;
        float colOpsX   = boxX + 606.0f;

        ColorRGBA colHdrColor = { 140, 150, 172, 255 };
        Renderer2D::DrawLabel("COMPONENT",      colCompX,  colHeaderY, 14, colHdrColor);
        Renderer2D::DrawLabel("ACTIVE",         colInstX,  colHeaderY, 14, colHdrColor);
        Renderer2D::DrawLabel("CPU TIME",       colCpuX,   colHeaderY, 14, colHdrColor);
        Renderer2D::DrawLabel("CPU LOAD",       colShareX, colHeaderY, 14, colHdrColor);
        Renderer2D::DrawLabel("RAM (ACT/TOT)",  colRamX,   colHeaderY, 14, colHdrColor);
        Renderer2D::DrawLabel("VRAM",           colVramX,  colHeaderY, 14, colHdrColor);
        Renderer2D::DrawLabel("OPS",            colOpsX,   colHeaderY, 14, colHdrColor);

        Renderer2D::DrawLine2D(boxX + 6.0f, colHeaderY + 18.0f, boxX + boxW - 6.0f, colHeaderY + 18.0f, { 40, 46, 60, 255 });

        // 3. Per-Component Consumption Rows
        struct RowEntry {
            const char* label;
            const ComponentDiagnostics* data;
            const char* opsUnit;
            ColorRGBA accent;
        };

        const RowEntry rows[7] = {
            { "Transform 2D", &diag.transform, "bounds", { 95,  195, 255, 255 } },
            { "Movement 2D",  &diag.movement,  "moving", { 105, 230, 145, 255 } },
            { "Collider 2D",  &diag.collider,  "AABB",   { 250, 200, 75,  255 } },
            { "Camera 2D",    &diag.camera,    "tracks", { 110, 225, 215, 255 } },
            { "Audio Source", &diag.audio,     "plays",  { 255, 165, 110, 255 } },
            { "Sprite 2D",    &diag.sprite,    "draws",  { 245, 130, 95,  255 } },
            { "Text Label",   &diag.text,      "draws",  { 195, 135, 255, 255 } }
        };

        double totalCompCpuUs = diag.transform.cpuTimeUs +
                                diag.movement.cpuTimeUs +
                                diag.collider.cpuTimeUs +
                                diag.camera.cpuTimeUs +
                                diag.audio.cpuTimeUs +
                                diag.sprite.cpuTimeUs +
                                diag.text.cpuTimeUs;
        size_t totalActiveRam = 0;
        size_t totalCompRam   = 0;
        size_t totalActiveVram = diag.sprite.vramBytes + diag.text.vramBytes;

        float rowY = colHeaderY + 21.0f;
        constexpr float rowH = 22.0f;

        for (int i = 0; i < 6; ++i) {
            const RowEntry& r = rows[i];
            const ComponentDiagnostics& cd = *r.data;

            totalActiveRam += cd.activeRamBytes;
            totalCompRam   += cd.ramBytes;

            ColorRGBA stripeBg = (i % 2 == 0)
                ? ColorRGBA{ 20, 23, 31, 210 }
                : ColorRGBA{ 16, 18, 25, 210 };
            Renderer2D::DrawRect(boxX + 4.0f, rowY, boxW - 8.0f, rowH - 1.0f, stripeBg);

            Renderer2D::DrawRect(colCompX - 4.0f, rowY + 3.0f, 3.0f, rowH - 7.0f, r.accent);
            Renderer2D::DrawLabel(r.label, colCompX + 3.0f, rowY + 3.0f, 14, r.accent);

            char instBuf[32];
            std::snprintf(instBuf, sizeof(instBuf), "%zu / %zu", cd.activeCount, cd.totalCount);
            Renderer2D::DrawLabel(instBuf, colInstX, rowY + 3.0f, 14, { 220, 225, 238, 255 });

            char cpuBuf[40];
            std::snprintf(cpuBuf, sizeof(cpuBuf), "%.2f us (%.3fms)", cd.cpuTimeUs, cd.cpuTimeUs / 1000.0);
            Renderer2D::DrawLabel(cpuBuf, colCpuX, rowY + 3.0f, 14, { 235, 240, 250, 255 });

            float shareRatio = (totalCompCpuUs > 0.001)
                ? static_cast<float>(cd.cpuTimeUs / totalCompCpuUs)
                : 0.0f;
            shareRatio = std::max(0.0f, std::min(1.0f, shareRatio));

            float barW = 46.0f;
            float barH = 8.0f;
            float barY = rowY + 7.0f;
            Renderer2D::DrawRect(colShareX, barY, barW, barH, { 32, 36, 48, 255 });
            if (shareRatio > 0.01f) {
                Renderer2D::DrawRect(colShareX, barY, std::max(2.0f, barW * shareRatio), barH, r.accent);
            }
            char pctBuf[16];
            std::snprintf(pctBuf, sizeof(pctBuf), "%3.0f%%", shareRatio * 100.0f);
            Renderer2D::DrawLabel(pctBuf, colShareX + barW + 5.0f, rowY + 3.0f, 14, { 185, 195, 215, 255 });

            char actRamStr[24], totRamStr[24], ramBuf[56];
            FormatBytes(cd.activeRamBytes, actRamStr, sizeof(actRamStr));
            FormatBytes(cd.ramBytes,       totRamStr, sizeof(totRamStr));
            std::snprintf(ramBuf, sizeof(ramBuf), "%s / %s", actRamStr, totRamStr);
            Renderer2D::DrawLabel(ramBuf, colRamX, rowY + 3.0f, 14, { 210, 220, 235, 255 });

            char vramBuf[24];
            FormatBytes(cd.vramBytes, vramBuf, sizeof(vramBuf));
            ColorRGBA vramColor = (cd.vramBytes > 0)
                ? ColorRGBA{ 125, 220, 255, 255 }
                : ColorRGBA{ 105, 112, 128, 255 };
            Renderer2D::DrawLabel(vramBuf, colVramX, rowY + 3.0f, 14, vramColor);

            char opsBuf[32];
            std::snprintf(opsBuf, sizeof(opsBuf), "%zu %s", cd.operationsCount, r.opsUnit);
            Renderer2D::DrawLabel(opsBuf, colOpsX, rowY + 3.0f, 14, { 175, 185, 205, 255 });

            rowY += rowH;
        }

        // 4. Totals Summary Row
        Renderer2D::DrawLine2D(boxX + 6.0f, rowY + 1.0f, boxX + boxW - 6.0f, rowY + 1.0f, { 55, 85, 120, 200 });
        rowY += 4.0f;

        char totActiveRamStr[24], totSceneRamStr[24], totVramStr[24];
        FormatBytes(totalActiveRam,          totActiveRamStr, sizeof(totActiveRamStr));
        FormatBytes(diag.totalSceneRamBytes, totSceneRamStr,  sizeof(totSceneRamStr));
        FormatBytes(totalActiveVram,         totVramStr,      sizeof(totVramStr));

        char summaryLine[180];
        std::snprintf(
            summaryLine, sizeof(summaryLine),
            "TOTALS  |  Update: %.2f us  +  Render: %.2f us  =  %.2f us   |   Comp RAM: %s (Scene: %s)   |   VRAM: %s",
            diag.totalUpdateUs, diag.totalRenderUs,
            diag.totalUpdateUs + diag.totalRenderUs,
            totActiveRamStr, totSceneRamStr, totVramStr
        );
        Renderer2D::DrawLabel(summaryLine, boxX + 10.0f, rowY + 2.0f, 14, { 135, 225, 170, 255 });
        rowY += 21.0f;

        // 5. Selected Entity Exact Per-Component Breakdown Row
        Renderer2D::DrawRect(boxX + 4.0f, rowY, boxW - 8.0f, 22.0f, { 22, 26, 36, 235 });
        const Entity* sel = scene.FindEntityById(selectedEntityId);
        if (sel) {
            size_t tRam = sizeof(TransformComponent);
            size_t mRam = sizeof(MovementComponent);
            size_t cRam = sizeof(ColliderComponent);
            size_t camRam = sizeof(CameraComponent) + sel->camera.targetName.capacity();
            size_t sRam = sizeof(SpriteComponent) + sel->sprite.texturePath.capacity();
            size_t xRam = sizeof(TextComponent)   + sel->text.content.capacity();
            size_t sVram = (sel->sprite.enabled && !sel->sprite.texturePath.empty())
                ? Renderer2D::GetTextureVRAMBytes(sel->sprite.texturePath.c_str())
                : 0;
            size_t xVram = sel->text.enabled
                ? Renderer2D::GetFontAtlasVRAMBytes(sel->text.fontSize)
                : 0;

            char sVramStr[24], xVramStr[24];
            FormatBytes(sVram, sVramStr, sizeof(sVramStr));
            FormatBytes(xVram, xVramStr, sizeof(xVramStr));

            char entityLine[240];
            std::snprintf(
                entityLine, sizeof(entityLine),
                "Entity '%s' (#%u): Trans %zuB | Move %zuB(%s) | Coll %zuB(%s) | Cam %zuB(%s) | Sprite %zuB+%s(%s) | Text %zuB+%s(%s)",
                sel->name.c_str(), sel->id,
                tRam,
                mRam, sel->movement.enabled ? "ON" : "OFF",
                cRam, sel->collider.enabled ? "ON" : "OFF",
                camRam, sel->camera.enabled ? "ON" : "OFF",
                sRam, sVramStr, sel->sprite.enabled ? "ON" : "OFF",
                xRam, xVramStr, sel->text.enabled   ? "ON" : "OFF"
            );
            Renderer2D::DrawLabel(entityLine, boxX + 10.0f, rowY + 3.0f, 14, { 255, 215, 90, 255 });
        } else {
            Renderer2D::DrawLabel(
                "Selected Entity: None (Select an entity in the Hierarchy to inspect its exact per-component byte footprint)",
                boxX + 10.0f, rowY + 3.0f, 14, { 145, 152, 170, 255 }
            );
        }

        Renderer2D::EndViewportScissor();
    }

}
