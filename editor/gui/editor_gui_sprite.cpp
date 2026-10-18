#include "editor/gui/editor_gui.h"
#include "core/input.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Scene;

    void EditorGui::DrawSortingControls(float x, float y, float w, int& sortingLayer, int& zOrder, int zSliderId) {
        Renderer2D::DrawLabel("Layer:", x, y + 3.0f, 14, { 185, 190, 205, 255 });

        float btnStartX = x + 54.0f;
        float availW = w - 54.0f;
        float gap = 4.0f;
        float btnW = (availW - gap * (SortingLayer::Count - 1)) / static_cast<float>(SortingLayer::Count);

        for (int i = 0; i < SortingLayer::Count; ++i) {
            float bx = btnStartX + i * (btnW + gap);
            bool isCurrent = (sortingLayer == i);

            ColorRGBA bg = isCurrent ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 };
            ColorRGBA hover = isCurrent ? ColorRGBA{ 68, 135, 220, 255 } : ColorRGBA{ 54, 58, 74, 255 };

            if (DrawButton(bx, y, btnW, 21.0f, SortingLayer::GetShortName(i), bg, hover)) {
                if (!isCurrent) {
                    PushUndoIfActive();
                }
                sortingLayer = i;
                m_focusedTextFieldId = -1;
            }
        }

        // Z-Order row: label + [-] step button + slider (-100..100) + [+] step button
        float zy = y + 25.0f;
        Renderer2D::DrawLabel("Z-Order", x, zy + 2.0f, 14, { 185, 190, 205, 255 });

        float minusX = x + 64.0f;
        float stepBtnW = 22.0f;
        float barH = 18.0f;

        if (DrawButton(minusX, zy, stepBtnW, barH, "-", { 42, 46, 58, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            zOrder = std::max(-100, zOrder - 1);
            m_focusedTextFieldId = -1;
        }

        float plusX = x + w - stepBtnW;
        if (DrawButton(plusX, zy, stepBtnW, barH, "+", { 42, 46, 58, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            zOrder = std::min(100, zOrder + 1);
            m_focusedTextFieldId = -1;
        }

        float barX = minusX + stepBtnW + 4.0f;
        float barW = plusX - barX - 4.0f;

        if (IsMouseOver(barX, zy, barW, barH) && Input::IsMouseButtonPressed(MouseButton::Left)) {
            BeginContinuousIfActive();
            m_activeSliderId = zSliderId;
            m_focusedTextFieldId = -1;
        }

        if (m_activeSliderId == zSliderId && Input::IsMouseButtonDown(MouseButton::Left)) {
            float t = (Input::GetMouseX() - barX) / barW;
            t = std::max(0.0f, std::min(1.0f, t));
            zOrder = static_cast<int>(std::round(-100.0f + t * 200.0f));
        }

        float fillRatio = (static_cast<float>(zOrder) + 100.0f) / 200.0f;
        fillRatio = std::max(0.0f, std::min(1.0f, fillRatio));

        Renderer2D::DrawRect(barX, zy, barW, barH, { 36, 38, 48, 255 });
        Renderer2D::DrawRect(barX, zy, barW * fillRatio, barH, { 65, 135, 220, 255 });
        Renderer2D::DrawRectOutline(barX, zy, barW, barH, 1.0f, { 70, 75, 90, 255 });

        char zBuf[32];
        std::snprintf(zBuf, sizeof(zBuf), "%d", zOrder);
        Renderer2D::DrawLabel(zBuf, barX + 8.0f, zy + 2.0f, 14, { 245, 245, 250, 255 });
    }

    void EditorGui::DrawTextureSelector(float x, float y, float w, Scene::SpriteComponent& sprite) {
        Renderer2D::DrawLabel("Preset:", x, y + 4.0f, 14, { 185, 190, 205, 255 });

        float btnStartX = x + 54.0f;
        float availW = w - 54.0f;
        float gap = 3.0f;
        float btnW = (availW - gap * (NUM_TEXTURE_PRESETS - 1)) / static_cast<float>(NUM_TEXTURE_PRESETS);

        for (int i = 0; i < NUM_TEXTURE_PRESETS; ++i) {
            float bx = btnStartX + i * (btnW + gap);
            bool isCurrent = (sprite.texturePath == TEXTURE_PRESETS[i].path);

            ColorRGBA bg = isCurrent ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 };
            ColorRGBA hover = isCurrent ? ColorRGBA{ 68, 135, 220, 255 } : ColorRGBA{ 54, 58, 74, 255 };

            if (DrawButton(bx, y, btnW, 22.0f, TEXTURE_PRESETS[i].label, bg, hover)) {
                if (!isCurrent) {
                    PushUndoIfActive();
                }

                bool wasSolidBox = sprite.texturePath.empty();
                sprite.texturePath = TEXTURE_PRESETS[i].path;
                m_focusedTextFieldId = -1;

                // When selecting the 256x64 4-frame sprite sheet preset, default to a 4-frame 64x64 animation
                if (std::string(TEXTURE_PRESETS[i].label) == "Sheet") {
                    sprite.sourceX = 0.0f;
                    sprite.sourceY = 0.0f;
                    sprite.sourceW = 64.0f;
                    sprite.sourceH = 64.0f;
                    sprite.frameCount = 4;
                    sprite.currentFrame = 0;
                    if (sprite.fps <= 0.0f) sprite.fps = 8.0f;
                    sprite.loop = true;
                    sprite.isPlaying = true;
                    sprite.frameTimer = 0.0f;
                } else {
                    if (sprite.frameCount > 1) {
                        sprite.frameCount = 1;
                        sprite.currentFrame = 0;
                        sprite.isPlaying = false;
                        sprite.frameTimer = 0.0f;
                        sprite.sourceX = 0.0f;
                        sprite.sourceY = 0.0f;
                        sprite.sourceW = 0.0f;
                        sprite.sourceH = 0.0f;
                    } else {
                        int texW = 0, texH = 0;
                        if (Renderer2D::GetTextureSize(sprite.texturePath.c_str(), texW, texH) &&
                            sprite.sourceX >= static_cast<float>(texW)) {
                            sprite.sourceX = 0.0f;
                        }
                    }
                }

                if (wasSolidBox && !sprite.texturePath.empty()) {
                    sprite.color = { 255, 255, 255, 255 };
                } else if (!wasSolidBox && sprite.texturePath.empty() &&
                           sprite.color.r == 255 && sprite.color.g == 255 && sprite.color.b == 255) {
                    sprite.color = { 80, 180, 250, 255 };
                }
            }
        }

        DrawTextInputBox(x, y + 27.0f, w, "Image:", sprite.texturePath, 3);
    }

    void EditorGui::DrawSpriteRegionControls(float x, float y, float w, Scene::SpriteComponent& sprite, int baseSliderId) {
        // 1. Horizontal & Vertical Mirroring Toggles (Flip X / Flip Y)
        DrawCheckbox(x, y, "Flip X (Horiz)", sprite.flipX);
        DrawCheckbox(x + w * 0.52f, y, "Flip Y (Vert)", sprite.flipY);

        // 2. UV Sub-Rectangle Header + Quick Crop / Frame Step Buttons
        float uvHeaderY = y + 23.0f;
        int texW = 0, texH = 0;
        bool hasTex = Renderer2D::GetTextureSize(sprite.texturePath.c_str(), texW, texH);
        float maxW = (hasTex && texW > 0) ? static_cast<float>(texW) : 256.0f;
        float maxH = (hasTex && texH > 0) ? static_cast<float>(texH) : 256.0f;

        char headerLabel[48];
        if (hasTex) {
            std::snprintf(headerLabel, sizeof(headerLabel), "UV Sub-Rect (%dx%d)", texW, texH);
        } else {
            std::snprintf(headerLabel, sizeof(headerLabel), "UV Sub-Rect (px)");
        }
        Renderer2D::DrawLabel(headerLabel, x, uvHeaderY + 2.0f, 14, { 160, 200, 245, 255 });

        // Quick helper buttons on the right side of the UV header: [Full] [64x64] [<] [>]
        float btnH = 19.0f;
        float rightEdge = x + w;
        float stepBtnW = 22.0f;
        float cropBtnW = 44.0f;
        float fullBtnW = 38.0f;
        float gap = 3.0f;

        float nextBtnX = rightEdge - stepBtnW;
        float prevBtnX = nextBtnX - gap - stepBtnW;
        float cropBtnX = prevBtnX - gap - cropBtnW;
        float fullBtnX = cropBtnX - gap - fullBtnW;

        bool isFullUV = (sprite.sourceX == 0.0f && sprite.sourceY == 0.0f &&
                         sprite.sourceW <= 0.0f && sprite.sourceH <= 0.0f);
        if (DrawButton(fullBtnX, uvHeaderY, fullBtnW, btnH, "Full",
                       isFullUV ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                       { 64, 128, 210, 255 })) {
            PushUndoIfActive();
            sprite.sourceX = 0.0f;
            sprite.sourceY = 0.0f;
            sprite.sourceW = 0.0f;
            sprite.sourceH = 0.0f;
            sprite.frameCount = 1;
            sprite.currentFrame = 0;
            sprite.isPlaying = false;
            sprite.frameTimer = 0.0f;
            m_focusedTextFieldId = -1;
        }

        bool is64x64 = (std::fabs(sprite.sourceW - 64.0f) < 0.5f && std::fabs(sprite.sourceH - 64.0f) < 0.5f);
        if (DrawButton(cropBtnX, uvHeaderY, cropBtnW, btnH, "64x64",
                       is64x64 ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                       { 64, 128, 210, 255 })) {
            PushUndoIfActive();
            sprite.sourceW = 64.0f;
            sprite.sourceH = 64.0f;
            m_focusedTextFieldId = -1;
        }

        float stepW = (sprite.sourceW > 0.0f) ? sprite.sourceW : 64.0f;
        if (DrawButton(prevBtnX, uvHeaderY, stepBtnW, btnH, "<", { 40, 44, 56, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            if (sprite.frameCount > 1) {
                sprite.isPlaying = false;
                sprite.currentFrame = (sprite.currentFrame - 1 + sprite.frameCount) % sprite.frameCount;
                sprite.SyncFrameUV(texW, texH);
            } else {
                if (sprite.sourceW <= 0.0f) sprite.sourceW = 64.0f;
                if (sprite.sourceH <= 0.0f) sprite.sourceH = 64.0f;
                sprite.sourceX -= stepW;
                if (sprite.sourceX < 0.0f) {
                    sprite.sourceX = std::max(0.0f, maxW - stepW);
                }
            }
            m_focusedTextFieldId = -1;
        }

        if (DrawButton(nextBtnX, uvHeaderY, stepBtnW, btnH, ">", { 40, 44, 56, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            if (sprite.frameCount > 1) {
                sprite.isPlaying = false;
                sprite.currentFrame = (sprite.currentFrame + 1) % sprite.frameCount;
                sprite.SyncFrameUV(texW, texH);
            } else {
                if (sprite.sourceW <= 0.0f) sprite.sourceW = 64.0f;
                if (sprite.sourceH <= 0.0f) sprite.sourceH = 64.0f;
                sprite.sourceX += stepW;
                if (sprite.sourceX >= maxW) {
                    sprite.sourceX = 0.0f;
                }
            }
            m_focusedTextFieldId = -1;
        }

        // 3. UV Sub-Rectangle Sliders (Source X, Source Y, Source W, Source H)
        float sy = uvHeaderY + 22.0f;
        DrawFloatSlider(x, sy,         w, "Source X", sprite.sourceX, 0.0f, maxW, baseSliderId + 0);
        DrawFloatSlider(x, sy + 20.0f, w, "Source Y", sprite.sourceY, 0.0f, maxH, baseSliderId + 1);
        DrawFloatSlider(x, sy + 40.0f, w, "Source W", sprite.sourceW, 0.0f, maxW, baseSliderId + 2);
        DrawFloatSlider(x, sy + 60.0f, w, "Source H", sprite.sourceH, 0.0f, maxH, baseSliderId + 3);

        sprite.sourceX = std::round(sprite.sourceX);
        sprite.sourceY = std::round(sprite.sourceY);
        sprite.sourceW = std::round(sprite.sourceW);
        sprite.sourceH = std::round(sprite.sourceH);
    }

    void EditorGui::DrawSpriteAnimatorControls(float x, float y, float w, Scene::SpriteComponent& sprite, int baseSliderId) {
        int texW = 0, texH = 0;
        bool hasTex = Renderer2D::GetTextureSize(sprite.texturePath.c_str(), texW, texH);

        sprite.frameCount = std::max(1, sprite.frameCount);
        sprite.currentFrame = std::max(0, std::min(sprite.currentFrame, sprite.frameCount - 1));

        // 1. Section Sub-Header + Live Frame/FPS Badge
        Renderer2D::DrawLabel("2D Sprite Animator", x, y + 1.0f, 14, { 110, 225, 195, 255 });

        char badgeBuf[48];
        if (sprite.frameCount > 1) {
            std::snprintf(badgeBuf, sizeof(badgeBuf), "[F %d/%d | %.0f FPS]",
                          sprite.currentFrame + 1, sprite.frameCount, sprite.fps);
        } else {
            std::snprintf(badgeBuf, sizeof(badgeBuf), "[Static 1f]");
        }
        int badgeW = Renderer2D::MeasureTextWidth(badgeBuf, 14);
        Renderer2D::DrawLabel(badgeBuf, x + w - static_cast<float>(badgeW), y + 1.0f, 14,
                              sprite.isPlaying ? ColorRGBA{ 105, 235, 150, 255 } : ColorRGBA{ 155, 165, 185, 255 });

        // 2. Playback Row: [Playing] checkbox, [Loop] checkbox, and [|<] [<] [>] transport buttons
        float row2Y = y + 21.0f;
        bool wasPlaying = sprite.isPlaying;
        if (DrawCheckbox(x, row2Y, "Playing", sprite.isPlaying)) {
            if (!wasPlaying && sprite.isPlaying) {
                if (sprite.frameCount <= 1) {
                    if (hasTex && texH > 0 && texW > texH && (texW % texH == 0)) {
                        sprite.frameCount = texW / texH;
                    } else {
                        sprite.frameCount = 4;
                    }
                    if (hasTex && texW > 0) {
                        sprite.sourceW = std::round(static_cast<float>(texW) / static_cast<float>(sprite.frameCount));
                        if (sprite.sourceH <= 0.0f) sprite.sourceH = static_cast<float>(texH);
                    }
                }
                if (!sprite.loop && sprite.currentFrame >= sprite.frameCount - 1) {
                    sprite.currentFrame = 0;
                }
                sprite.frameTimer = 0.0f;
                sprite.SyncFrameUV(texW, texH);
            }
        }

        DrawCheckbox(x + 96.0f, row2Y, "Loop", sprite.loop);

        float btnH = 18.0f;
        float stepBtnW = 24.0f;
        float gap = 3.0f;
        float rightEdge = x + w;
        float nextFrameX = rightEdge - stepBtnW;
        float prevFrameX = nextFrameX - gap - stepBtnW;
        float firstFrameX = prevFrameX - gap - 26.0f;

        if (DrawButton(firstFrameX, row2Y, 26.0f, btnH, "|<", { 40, 44, 56, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            sprite.currentFrame = 0;
            sprite.frameTimer = 0.0f;
            sprite.SyncFrameUV(texW, texH);
            m_focusedTextFieldId = -1;
        }
        if (DrawButton(prevFrameX, row2Y, stepBtnW, btnH, "<", { 40, 44, 56, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            sprite.isPlaying = false;
            sprite.frameTimer = 0.0f;
            if (sprite.frameCount > 1) {
                sprite.currentFrame = (sprite.currentFrame - 1 + sprite.frameCount) % sprite.frameCount;
                sprite.SyncFrameUV(texW, texH);
            }
            m_focusedTextFieldId = -1;
        }
        if (DrawButton(nextFrameX, row2Y, stepBtnW, btnH, ">", { 40, 44, 56, 255 }, { 62, 68, 86, 255 })) {
            PushUndoIfActive();
            sprite.isPlaying = false;
            sprite.frameTimer = 0.0f;
            if (sprite.frameCount > 1) {
                sprite.currentFrame = (sprite.currentFrame + 1) % sprite.frameCount;
                sprite.SyncFrameUV(texW, texH);
            }
            m_focusedTextFieldId = -1;
        }

        // 3. Frame Count, Current Frame Scrubber, and Playback Speed (FPS) Sliders
        float sy = row2Y + 23.0f;
        int prevFrameCount = sprite.frameCount;
        DrawIntSlider(x, sy, w, "Frames", sprite.frameCount, 1, 16, baseSliderId + 0);
        if (sprite.frameCount != prevFrameCount) {
            sprite.currentFrame = std::max(0, std::min(sprite.currentFrame, sprite.frameCount - 1));
            if (sprite.frameCount > 1) {
                if (hasTex && texW > 0) {
                    sprite.sourceW = std::round(static_cast<float>(texW) / static_cast<float>(sprite.frameCount));
                    if (sprite.sourceH <= 0.0f) sprite.sourceH = static_cast<float>(texH);
                } else if (sprite.sourceW <= 0.0f) {
                    sprite.sourceW = 64.0f;
                    sprite.sourceH = 64.0f;
                }
                sprite.SyncFrameUV(texW, texH);
            } else {
                sprite.currentFrame = 0;
                sprite.isPlaying = false;
            }
        }

        int prevFrame = sprite.currentFrame;
        int maxFrameIdx = std::max(1, sprite.frameCount - 1);
        DrawIntSlider(x, sy + 20.0f, w, "Frame #", sprite.currentFrame, 0, maxFrameIdx, baseSliderId + 1);
        sprite.currentFrame = std::max(0, std::min(sprite.currentFrame, sprite.frameCount - 1));
        if (sprite.currentFrame != prevFrame) {
            sprite.frameTimer = 0.0f;
            sprite.SyncFrameUV(texW, texH);
        }

        DrawFloatSlider(x, sy + 40.0f, w, "Speed FPS", sprite.fps, 1.0f, 30.0f, baseSliderId + 2);
        sprite.fps = std::round(sprite.fps);
    }

}
