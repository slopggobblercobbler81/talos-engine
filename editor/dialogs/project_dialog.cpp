// Modal Project Workflow dialog for creating, opening, and saving project.talos
#include "editor/dialogs/project_dialog.h"
#include "audio/audio_system.h"
#include "core/input_map.h"
#include "core/window.h"
#include "renderer/renderer2d.h"
#include "scene/scene_serializer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Talos::Editor {

    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Audio;
    using namespace Talos::Scene;
    using namespace Talos::Editor::EditorLayout;

    namespace {

        void SelectDefaultEntity(Scene::Scene& scene, Scene::EntityID& outSelectedId) {
            if (scene.GetEntities().empty()) {
                outSelectedId = 0;
                return;
            }
            Entity* player = scene.FindEntity("Player");
            outSelectedId = player ? player->id : scene.GetEntities().front().id;
        }

    } // anonymous namespace

    bool ProjectDialog::Initialize(const std::string& projectFilePath, std::string& inOutSceneFilePath) {
        m_projectFilePath = projectFilePath.empty() ? "examples/project.talos" : projectFilePath;

        if (ProjectSerializer::Load(m_config, m_projectFilePath, true)) {
            inOutSceneFilePath = ProjectSerializer::ResolveScenePath(m_projectFilePath, m_config.startScene);
            return true;
        }

        m_config = ProjectSerializer::MakeDefaultConfig();
        if (!inOutSceneFilePath.empty()) {
            m_config.startScene = inOutSceneFilePath;
        }
        ProjectSerializer::CaptureFromEngine(m_config);
        ProjectSerializer::Save(m_config, m_projectFilePath);
        ProjectSerializer::ApplyToEngine(m_config);
        return false;
    }

    void ProjectDialog::BindTextFields(EditorGui& gui) {
        gui.BindExternalTextField(10, &m_projectFilePath);
        gui.BindExternalTextField(11, &m_config.name);
        gui.BindExternalTextField(12, &m_config.version);
        gui.BindExternalTextField(13, &m_config.startScene);
    }

    void ProjectDialog::Toggle(EditorGui& gui) {
        m_isOpen = !m_isOpen;
        if (m_isOpen) {
            ProjectSerializer::CaptureFromEngine(m_config);
            BindTextFields(gui);
        } else {
            gui.CancelSliderDrag();
            if (gui.GetFocusedTextFieldId() >= 10) {
                gui.UnfocusTextField();
            }
        }
    }

    void ProjectDialog::Close(EditorGui& gui) {
        m_isOpen = false;
        gui.CancelSliderDrag();
        if (gui.GetFocusedTextFieldId() >= 10) {
            gui.UnfocusTextField();
        }
    }

    bool ProjectDialog::CreateNewProject(Scene::Scene& scene,
                                         std::string& inOutSceneFilePath,
                                         Scene::EntityID& inOutSelectedEntityId,
                                         EditorUndoRedo& undoRedo,
                                         std::string& outStatusMessage) {
        if (ProjectSerializer::CreateNewProject(m_config, scene, m_projectFilePath, true)) {
            inOutSceneFilePath = ProjectSerializer::ResolveScenePath(m_projectFilePath, m_config.startScene);
            undoRedo.Clear();
            SelectDefaultEntity(scene, inOutSelectedEntityId);
            outStatusMessage = "Created Project -> " + m_projectFilePath;
            return true;
        }
        outStatusMessage = "Failed to create project at " + m_projectFilePath;
        return false;
    }

    bool ProjectDialog::OpenProject(Scene::Scene& scene,
                                    std::string& inOutSceneFilePath,
                                    Scene::EntityID& inOutSelectedEntityId,
                                    EditorUndoRedo& undoRedo,
                                    std::string& outStatusMessage) {
        if (ProjectSerializer::OpenProject(m_config, scene, m_projectFilePath)) {
            inOutSceneFilePath = ProjectSerializer::ResolveScenePath(m_projectFilePath, m_config.startScene);
            undoRedo.Clear();
            SelectDefaultEntity(scene, inOutSelectedEntityId);
            outStatusMessage = "Opened Project <- " + m_projectFilePath;
            return true;
        }
        outStatusMessage = "Project file not found: " + m_projectFilePath;
        return false;
    }

    bool ProjectDialog::SaveProject(const Scene::Scene& scene,
                                    const std::string& activeScenePath,
                                    std::string& outStatusMessage) {
        if (ProjectSerializer::SaveProject(m_config, &scene, m_projectFilePath, activeScenePath, true)) {
            outStatusMessage = "Saved Project -> " + m_projectFilePath;
            return true;
        }
        outStatusMessage = "Failed to save project: " + m_projectFilePath;
        return false;
    }

    void ProjectDialog::SyncSubsystemsToProjectFile() {
        ProjectSerializer::CaptureFromEngine(m_config);
        ProjectSerializer::Save(m_config, m_projectFilePath);
    }

    void ProjectDialog::Render(Scene::Scene& scene,
                               int windowWidth,
                               int windowHeight,
                               float consoleHeight,
                               std::string& inOutSceneFilePath,
                               Scene::EntityID& inOutSelectedEntityId,
                               EditorGui& gui,
                               EditorUndoRedo& undoRedo,
                               bool& outRequestOpenInputMap,
                               bool& outRequestOpenAudioMixer,
                               std::string& outStatusMessage) {
        outRequestOpenInputMap   = false;
        outRequestOpenAudioMixer = false;
        BindTextFields(gui);

        float vpX = LEFT_PANEL_WIDTH;
        float vpY = TOOLBAR_HEIGHT;
        float vpW = static_cast<float>(windowWidth) - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
        float vpH = static_cast<float>(windowHeight) - TOOLBAR_HEIGHT - consoleHeight;

        // Translucent dark backdrop over the central viewport
        Renderer2D::DrawRect(vpX, vpY, vpW, vpH, { 10, 11, 16, 205 });

        float modalW = std::min(720.0f, static_cast<float>(windowWidth) - 80.0f);
        float modalH = std::min(504.0f, static_cast<float>(windowHeight) - TOOLBAR_HEIGHT - 36.0f);
        float modalX = std::round((static_cast<float>(windowWidth) - modalW) * 0.5f);
        float modalY = std::round(TOOLBAR_HEIGHT + std::max(12.0f, (static_cast<float>(windowHeight) - TOOLBAR_HEIGHT - modalH) * 0.42f));

        // Modal shadow, card background, and accent border
        Renderer2D::DrawRect(modalX + 4.0f, modalY + 4.0f, modalW, modalH, { 5, 6, 10, 160 });
        Renderer2D::DrawRect(modalX, modalY, modalW, modalH, { 22, 24, 31, 255 });
        Renderer2D::DrawRectOutline(modalX, modalY, modalW, modalH, 2.0f, { 75, 145, 225, 255 });

        // 1. Modal Header Bar & Primary Project Workflow Actions (Create / Open / Save)
        constexpr float headerH = 36.0f;
        Renderer2D::DrawRect(modalX + 2.0f, modalY + 2.0f, modalW - 4.0f, headerH, { 28, 32, 42, 255 });
        Renderer2D::DrawLine2D(modalX, modalY + headerH + 2.0f, modalX + modalW, modalY + headerH + 2.0f, { 55, 65, 85, 255 });

        Renderer2D::DrawLabel("PROJECT WORKFLOW  -  project.talos", modalX + 14.0f, modalY + 11.0f, 15, { 95, 200, 255, 255 });

        float btnY = modalY + 7.0f;
        if (gui.DrawButton(modalX + modalW - 392.0f, btnY, 96.0f, 24.0f, "New Project",
                           { 45, 115, 195, 255 }, { 62, 140, 225, 255 })) {
            gui.UnfocusTextField();
            CreateNewProject(scene, inOutSceneFilePath, inOutSelectedEntityId, undoRedo, outStatusMessage);
        }
        if (gui.DrawButton(modalX + modalW - 290.0f, btnY, 98.0f, 24.0f, "Open Project",
                           { 70, 76, 98, 255 }, { 92, 100, 126, 255 })) {
            gui.UnfocusTextField();
            OpenProject(scene, inOutSceneFilePath, inOutSelectedEntityId, undoRedo, outStatusMessage);
        }
        if (gui.DrawButton(modalX + modalW - 186.0f, btnY, 98.0f, 24.0f, "Save Project",
                           { 42, 138, 86, 255 }, { 56, 168, 106, 255 })) {
            gui.UnfocusTextField();
            SaveProject(scene, inOutSceneFilePath, outStatusMessage);
        }
        if (gui.DrawButton(modalX + modalW - 82.0f, btnY, 70.0f, 24.0f, "Close [X]",
                           { 140, 48, 54, 255 }, { 175, 64, 70, 255 })) {
            Close(gui);
            return;
        }

        float cx = modalX + 16.0f;
        float cw = modalW - 32.0f;
        float cy = modalY + headerH + 12.0f;

        // 2. Project File Path Row
        Renderer2D::DrawLabel("1. UNIFIED PROJECT FILE LOCATION", cx, cy, 13, { 115, 185, 245, 255 });
        cy += 18.0f;

        gui.DrawTextInputBox(cx, cy, cw - 208.0f, "File Path:", m_projectFilePath, 10);
        bool isExProj     = (m_projectFilePath == "examples/project.talos");
        bool isScenesProj = (m_projectFilePath == "examples/scenes/project.talos");
        if (gui.DrawButton(cx + cw - 200.0f, cy, 96.0f, 23.0f, "examples/...",
                           isExProj ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                           { 68, 135, 220, 255 })) {
            m_projectFilePath = "examples/project.talos";
        }
        if (gui.DrawButton(cx + cw - 100.0f, cy, 100.0f, 23.0f, "scenes/...",
                           isScenesProj ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                           { 68, 135, 220, 255 })) {
            m_projectFilePath = "examples/scenes/project.talos";
        }
        cy += 30.0f;
        Renderer2D::DrawLine2D(cx, cy, cx + cw, cy, { 42, 46, 58, 255 });
        cy += 8.0f;

        // 3. General Project Settings ([Project])
        Renderer2D::DrawLabel("2. GENERAL PROJECT CONFIGURATION ([Project])", cx, cy, 13, { 115, 185, 245, 255 });
        cy += 18.0f;

        float halfW = (cw - 14.0f) * 0.5f;
        gui.DrawTextInputBox(cx, cy, halfW + 70.0f, "Game Title:", m_config.name, 11);
        gui.DrawTextInputBox(cx + halfW + 84.0f, cy, halfW - 70.0f, "Version:", m_config.version, 12);
        cy += 30.0f;

        int prevW   = m_config.windowWidth;
        int prevH   = m_config.windowHeight;
        int prevFps = m_config.targetFps;

        gui.DrawIntSlider(cx, cy, halfW, "Window Width", m_config.windowWidth, 320, 3840, 130);
        gui.DrawIntSlider(cx + halfW + 14.0f, cy, halfW, "Window Height", m_config.windowHeight, 240, 2160, 131);
        cy += 24.0f;

        // Resolution quick presets
        Renderer2D::DrawLabel("Presets:", cx, cy + 3.0f, 13, { 150, 156, 175, 255 });
        struct ResPreset { const char* label; int w; int h; };
        constexpr ResPreset resPresets[5] = {
            { "1280x720 HD",      1280, 720  },
            { "1360x780 Editor",  1360, 780  },
            { "1600x900 HD+",     1600, 900  },
            { "1920x1080 FHD",    1920, 1080 },
            { "800x600 Retro",    800,  600  }
        };
        float rx = cx + 64.0f;
        float rw = (cw - 64.0f - 24.0f) / 5.0f;
        for (int i = 0; i < 5; ++i) {
            bool activeRes = (m_config.windowWidth == resPresets[i].w && m_config.windowHeight == resPresets[i].h);
            if (gui.DrawButton(rx, cy, rw, 20.0f, resPresets[i].label,
                               activeRes ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 42, 54, 255 },
                               { 68, 135, 220, 255 })) {
                m_config.windowWidth  = resPresets[i].w;
                m_config.windowHeight = resPresets[i].h;
            }
            rx += rw + 6.0f;
        }
        cy += 28.0f;

        gui.DrawIntSlider(cx, cy, halfW, "Target FPS", m_config.targetFps, 15, 240, 132);
        if (gui.DrawButton(cx + halfW + 14.0f, cy, 56.0f, 20.0f, "60 FPS",
                           m_config.targetFps == 60 ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 42, 54, 255 },
                           { 68, 135, 220, 255 })) {
            m_config.targetFps = 60;
        }
        if (gui.DrawButton(cx + halfW + 76.0f, cy, 62.0f, 20.0f, "120 FPS",
                           m_config.targetFps == 120 ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 42, 54, 255 },
                           { 68, 135, 220, 255 })) {
            m_config.targetFps = 120;
        }
        gui.DrawCheckbox(cx + halfW + 150.0f, cy + 1.0f, "Fullscreen", m_config.fullscreen);
        gui.DrawCheckbox(cx + halfW + 250.0f, cy + 1.0f, "Resizable", m_config.resizable);

        if (m_config.targetFps != prevFps) {
            Window::SetTargetFrameRate(m_config.targetFps);
        }
        (void)prevW;
        (void)prevH;

        cy += 28.0f;
        Renderer2D::DrawLine2D(cx, cy, cx + cw, cy, { 42, 46, 58, 255 });
        cy += 8.0f;

        // 4. Initial Scene Configuration (startScene)
        Renderer2D::DrawLabel("3. INITIAL SCENE & STARTUP WORKFLOW (startScene)", cx, cy, 13, { 115, 185, 245, 255 });
        cy += 18.0f;

        gui.DrawTextInputBox(cx, cy, cw - 210.0f, "Start Scene:", m_config.startScene, 13);
        bool isMainScene = (m_config.startScene == "examples/scenes/main.talsc");
        bool isDemoScene = (m_config.startScene == "examples/scenes/demo_scene.talsc");
        if (gui.DrawButton(cx + cw - 202.0f, cy, 96.0f, 23.0f, "main.talsc",
                           isMainScene ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                           { 68, 135, 220, 255 })) {
            m_config.startScene = "examples/scenes/main.talsc";
        }
        if (gui.DrawButton(cx + cw - 102.0f, cy, 102.0f, 23.0f, "demo_scene",
                           isDemoScene ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                           { 68, 135, 220, 255 })) {
            m_config.startScene = "examples/scenes/demo_scene.talsc";
        }
        cy += 28.0f;

        float actionBtnW = (cw - 16.0f) / 3.0f;
        if (gui.DrawButton(cx, cy, actionBtnW, 24.0f, "Use Active Editor Scene",
                           { 48, 82, 135, 255 }, { 64, 106, 175, 255 })) {
            m_config.startScene = inOutSceneFilePath;
            outStatusMessage = "Start scene set to '" + m_config.startScene + "'";
        }
        if (gui.DrawButton(cx + actionBtnW + 8.0f, cy, actionBtnW, 24.0f, "Load Start Scene Now",
                           { 55, 95, 145, 255 }, { 72, 122, 185, 255 })) {
            std::string resolved = ProjectSerializer::ResolveScenePath(m_projectFilePath, m_config.startScene);
            undoRedo.PushUndoState(scene, inOutSelectedEntityId,
                                   gui.GetTextCursorPos(), gui.GetTextSelectionAnchor(), false);
            if (SceneSerializer::Load(scene, resolved)) {
                inOutSceneFilePath = resolved;
                SelectDefaultEntity(scene, inOutSelectedEntityId);
                outStatusMessage = "Loaded Start Scene <- " + resolved;
            } else {
                outStatusMessage = "Start scene not found: " + resolved;
            }
        }
        if (gui.DrawButton(cx + (actionBtnW + 8.0f) * 2.0f, cy, actionBtnW, 24.0f, "Save Active as Start Scene",
                           { 44, 125, 112, 255 }, { 58, 155, 140, 255 })) {
            std::string resolved = ProjectSerializer::ResolveScenePath(m_projectFilePath, m_config.startScene);
            if (SceneSerializer::Save(scene, resolved)) {
                inOutSceneFilePath = resolved;
                ProjectSerializer::SaveProject(m_config, nullptr, m_projectFilePath, "", true);
                outStatusMessage = "Saved Start Scene -> " + resolved;
            } else {
                outStatusMessage = "Failed to save scene to " + resolved;
            }
        }
        cy += 32.0f;
        Renderer2D::DrawLine2D(cx, cy, cx + cw, cy, { 42, 46, 58, 255 });
        cy += 8.0f;

        // 5. Embedded Subsystems ([Audio] & [InputMap])
        Renderer2D::DrawLabel("4. UNIFIED SUBSYSTEMS EMBEDDED IN project.talos ([Audio] & [InputMap])", cx, cy, 13, { 115, 185, 245, 255 });
        cy += 20.0f;

        int masterPct = static_cast<int>(std::round(AudioSystem::GetMasterVolume() * 100.0f));
        int sfxPct    = static_cast<int>(std::round(AudioSystem::GetSFXVolume() * 100.0f));
        int musicPct  = static_cast<int>(std::round(AudioSystem::GetMusicBusVolume() * 100.0f));
        int numActs   = static_cast<int>(InputMap::GetActions().size());

        char summaryBuf[160];
        std::snprintf(summaryBuf, sizeof(summaryBuf),
                      "Audio Buses: Master %d%% | SFX %d%% | Music %d%%     |     Input Map: %d actions",
                      masterPct, sfxPct, musicPct, numActs);
        Renderer2D::DrawLabel(summaryBuf, cx, cy + 4.0f, 14, { 195, 202, 218, 255 });

        if (gui.DrawButton(cx + cw - 224.0f, cy, 108.0f, 24.0f, "Audio Mixer...",
                           { 48, 95, 145, 255 }, { 65, 120, 180, 255 })) {
            Close(gui);
            outRequestOpenAudioMixer = true;
            return;
        }
        if (gui.DrawButton(cx + cw - 110.0f, cy, 110.0f, 24.0f, "Input Map...",
                           { 92, 68, 160, 255 }, { 115, 88, 192, 255 })) {
            Close(gui);
            outRequestOpenInputMap = true;
            return;
        }

        // 6. Modal Footer Bar
        float footerY = modalY + modalH - 28.0f;
        Renderer2D::DrawRect(modalX + 2.0f, footerY, modalW - 4.0f, 26.0f, { 18, 20, 26, 255 });
        Renderer2D::DrawLine2D(modalX + 2.0f, footerY, modalX + modalW - 2.0f, footerY, { 45, 50, 64, 255 });

        std::string footerText = "Active Editor Scene: " + inOutSceneFilePath +
                                 "   |   Shortcut: [Ctrl+S] saves Scene & " + m_projectFilePath;
        Renderer2D::DrawLabel(footerText.c_str(), modalX + 14.0f, footerY + 6.0f, 13, { 145, 152, 172, 255 });
    }

}
