#include "editor/docks/console_dock.h"
#include "core/input.h"
#include "core/logger.h"
#include "renderer/renderer2d.h"
#include <algorithm>
#include <cstdio>
#include <vector>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Editor::EditorLayout;

    float ConsoleDock::GetPanelHeight() const {
        return m_consoleOpen ? CONSOLE_HEIGHT : CONSOLE_HEADER_HEIGHT;
    }

    void ConsoleDock::Render(int windowWidth, int windowHeight, EditorGui& gui) {
        float consoleX = LEFT_PANEL_WIDTH;
        float consoleW = static_cast<float>(windowWidth) - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
        float consoleH = GetPanelHeight();
        float consoleY = static_cast<float>(windowHeight) - consoleH;

        // Console panel background & top divider border
        Renderer2D::DrawRect(consoleX, consoleY, consoleW, consoleH, { 18, 19, 24, 255 });
        Renderer2D::DrawLine2D(consoleX, consoleY, consoleX + consoleW, consoleY, { 58, 62, 76, 255 });

        // Header bar background
        Renderer2D::DrawRect(consoleX, consoleY, consoleW, CONSOLE_HEADER_HEIGHT, { 24, 26, 33, 255 });
        Renderer2D::DrawLine2D(consoleX, consoleY + CONSOLE_HEADER_HEIGHT,
                               consoleX + consoleW, consoleY + CONSOLE_HEADER_HEIGHT, { 42, 45, 56, 255 });

        // Title label
        Renderer2D::DrawLabel("DEBUG CONSOLE", consoleX + 12.0f, consoleY + 6.0f, 15, { 155, 165, 185, 255 });

        // Counts by message classification type
        size_t infoCount  = Core::Logger::GetCountByLevel(Core::LogLevel::Info);
        size_t warnCount  = Core::Logger::GetCountByLevel(Core::LogLevel::Warning);
        size_t errorCount = Core::Logger::GetCountByLevel(Core::LogLevel::Error);

        char infoBtnLabel[32];
        char warnBtnLabel[32];
        char errBtnLabel[32];
        std::snprintf(infoBtnLabel, sizeof(infoBtnLabel), "INFO: %zu", infoCount);
        std::snprintf(warnBtnLabel, sizeof(warnBtnLabel), "WARN: %zu", warnCount);
        std::snprintf(errBtnLabel,  sizeof(errBtnLabel),  "ERROR: %zu", errorCount);

        float filterX = consoleX + 148.0f;
        if (gui.DrawButton(filterX, consoleY + 3.0f, 82.0f, 22.0f, infoBtnLabel,
                           m_showLogInfo ? ColorRGBA{ 38, 88, 138, 255 } : ColorRGBA{ 32, 35, 44, 255 },
                           { 52, 112, 172, 255 })) {
            m_showLogInfo = !m_showLogInfo;
        }

        if (gui.DrawButton(filterX + 88.0f, consoleY + 3.0f, 86.0f, 22.0f, warnBtnLabel,
                           m_showLogWarn ? ColorRGBA{ 135, 95, 28, 255 } : ColorRGBA{ 32, 35, 44, 255 },
                           { 168, 120, 38, 255 })) {
            m_showLogWarn = !m_showLogWarn;
        }

        if (gui.DrawButton(filterX + 180.0f, consoleY + 3.0f, 92.0f, 22.0f, errBtnLabel,
                           m_showLogError ? ColorRGBA{ 148, 42, 48, 255 } : ColorRGBA{ 32, 35, 44, 255 },
                           { 182, 56, 62, 255 })) {
            m_showLogError = !m_showLogError;
        }

        // Right-aligned Clear and Collapse/Expand buttons
        float rightBtnX = consoleX + consoleW - 142.0f;
        if (gui.DrawButton(rightBtnX, consoleY + 3.0f, 64.0f, 22.0f, "Clear",
                           { 45, 48, 60, 255 }, { 65, 70, 88, 255 })) {
            Core::Logger::Clear();
            m_consoleScrollOffset = 0;
            m_lastLogCount = 0;
        }

        if (gui.DrawButton(rightBtnX + 70.0f, consoleY + 3.0f, 62.0f, 22.0f,
                           m_consoleOpen ? "Hide" : "Show",
                           { 45, 48, 60, 255 }, { 65, 70, 88, 255 })) {
            m_consoleOpen = !m_consoleOpen;
        }

        if (!m_consoleOpen) {
            return;
        }

        // Gather filtered entries
        const auto& allEntries = Core::Logger::GetEntries();
        std::vector<const Core::LogEntry*> filtered;
        filtered.reserve(allEntries.size());
        for (const auto& entry : allEntries) {
            if (entry.level == Core::LogLevel::Info    && !m_showLogInfo)  continue;
            if (entry.level == Core::LogLevel::Warning && !m_showLogWarn)  continue;
            if (entry.level == Core::LogLevel::Error   && !m_showLogError) continue;
            filtered.push_back(&entry);
        }

        // Auto-scroll to the newest message at the bottom when new logs arrive
        if (allEntries.size() != m_lastLogCount) {
            m_consoleScrollOffset = 0;
            m_lastLogCount = allEntries.size();
        }

        float bodyY = consoleY + CONSOLE_HEADER_HEIGHT;
        float bodyH = consoleH - CONSOLE_HEADER_HEIGHT;
        constexpr float rowHeight = 22.0f;
        int maxVisibleRows = std::max(1, static_cast<int>((bodyH - 6.0f) / rowHeight));
        int totalRows = static_cast<int>(filtered.size());
        int maxScroll = std::max(0, totalRows - maxVisibleRows);

        if (EditorGui::IsMouseOver(consoleX, bodyY, consoleW, bodyH)) {
            float wheel = Input::GetMouseWheelMove();
            if (wheel > 0.01f) {
                m_consoleScrollOffset = std::min(maxScroll, m_consoleScrollOffset + 1);
            } else if (wheel < -0.01f) {
                m_consoleScrollOffset = std::max(0, m_consoleScrollOffset - 1);
            }
        }
        m_consoleScrollOffset = std::max(0, std::min(m_consoleScrollOffset, maxScroll));

        Renderer2D::BeginViewportScissor(
            static_cast<int>(consoleX),
            static_cast<int>(bodyY),
            static_cast<int>(consoleW),
            static_cast<int>(bodyH)
        );

        if (filtered.empty()) {
            Renderer2D::DrawLabel(
                "No messages matching current filter.",
                consoleX + 14.0f, bodyY + 10.0f, 14, { 110, 115, 130, 255 }
            );
        } else {
            int endIdx   = totalRows - m_consoleScrollOffset;
            int startIdx = std::max(0, endIdx - maxVisibleRows);

            float rowY = bodyY + 3.0f;
            for (int i = startIdx; i < endIdx; ++i) {
                const Core::LogEntry& entry = *filtered[i];

                ColorRGBA rowBg = (i % 2 == 0) ? ColorRGBA{ 22, 24, 30, 255 } : ColorRGBA{ 18, 19, 24, 255 };
                if (entry.level == Core::LogLevel::Error) {
                    rowBg = { 48, 20, 24, 255 };
                } else if (entry.level == Core::LogLevel::Warning) {
                    rowBg = { 44, 34, 18, 255 };
                }
                Renderer2D::DrawRect(consoleX + 2.0f, rowY, consoleW - 10.0f, rowHeight - 1.0f, rowBg);

                // 1. Classification Badge ([INFO], [WARN], [ERROR])
                ColorRGBA badgeColor = { 85, 190, 255, 255 };
                const char* badgeText = "[INFO]";
                if (entry.level == Core::LogLevel::Warning) {
                    badgeColor = { 255, 195, 65, 255 };
                    badgeText  = "[WARN]";
                } else if (entry.level == Core::LogLevel::Error) {
                    badgeColor = { 255, 95, 95, 255 };
                    badgeText  = "[ERROR]";
                }

                float curX = consoleX + 10.0f;
                Renderer2D::DrawLabel(badgeText, curX, rowY + 3.0f, 14, badgeColor);
                curX += 64.0f;

                // 2. Timestamp (HH:MM:SS)
                Renderer2D::DrawLabel(entry.timestamp.c_str(), curX, rowY + 3.0f, 14, { 120, 126, 142, 255 });
                curX += 66.0f;

                // 3. Source File & Line Number ([file:line])
                std::string locationTag = "[" + entry.file + ":" + std::to_string(entry.line) + "]";
                Renderer2D::DrawLabel(locationTag.c_str(), curX, rowY + 3.0f, 14, { 165, 180, 215, 255 });
                int locWidth = Renderer2D::MeasureTextWidth(locationTag.c_str(), 14);
                curX += static_cast<float>(locWidth) + 10.0f;

                // 4. Diagnostic Message Content
                ColorRGBA msgColor = { 230, 232, 240, 255 };
                if (entry.level == Core::LogLevel::Error) {
                    msgColor = { 255, 185, 185, 255 };
                } else if (entry.level == Core::LogLevel::Warning) {
                    msgColor = { 255, 228, 165, 255 };
                }
                Renderer2D::DrawLabel(entry.message.c_str(), curX, rowY + 3.0f, 14, msgColor);

                rowY += rowHeight;
            }

            if (totalRows > maxVisibleRows) {
                float trackX = consoleX + consoleW - 6.0f;
                float trackY = bodyY + 2.0f;
                float trackH = bodyH - 4.0f;
                Renderer2D::DrawRect(trackX, trackY, 4.0f, trackH, { 30, 32, 40, 255 });

                float thumbRatio = static_cast<float>(maxVisibleRows) / static_cast<float>(totalRows);
                float thumbH = std::max(16.0f, trackH * thumbRatio);
                float scrollFraction = static_cast<float>(maxScroll - m_consoleScrollOffset) / static_cast<float>(maxScroll);
                float thumbY = trackY + (trackH - thumbH) * scrollFraction;
                Renderer2D::DrawRect(trackX, thumbY, 4.0f, thumbH, { 95, 105, 130, 255 });
            }
        }

        Renderer2D::EndViewportScissor();
    }

}
