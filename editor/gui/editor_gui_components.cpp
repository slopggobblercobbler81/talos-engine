// EditorGui: collider, camera, and audio source inspector controls
#include "editor/gui/editor_gui.h"
#include "audio/audio_system.h"
#include "core/input.h"
#include <algorithm>
#include <cmath>

namespace Talos::Editor {

    using namespace Talos::Audio;
    using namespace Talos::Core;
    using namespace Talos::Renderer;
    using namespace Talos::Scene;

    float EditorGui::DrawColliderControls(float x, float y, float w,
                                          Scene::ColliderComponent& collider,
                                          const Scene::TransformComponent& transform,
                                          int baseSliderId) {
        float cy = y;

        // 1. Shape Toggle Row: [Box (AABB)] / [Circle]
        Renderer2D::DrawLabel("Shape:", x, cy + 3.0f, 14, { 185, 190, 205, 255 });

        bool isBox    = (collider.shape == Physics::ColliderShape::Box);
        bool isCircle = (collider.shape == Physics::ColliderShape::Circle);

        float btnStartX = x + 54.0f;
        float availW    = w - 54.0f;
        float gap       = 6.0f;
        float btnW      = (availW - gap) * 0.5f;

        if (DrawButton(btnStartX, cy, btnW, 20.0f, "Box (AABB)",
                       isBox ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 },
                       { 68, 135, 220, 255 })) {
            if (!isBox) {
                PushUndoIfActive();
            }
            collider.shape = Physics::ColliderShape::Box;
            m_focusedTextFieldId = -1;
        }

        if (DrawButton(btnStartX + btnW + gap, cy, btnW, 20.0f, "Circle",
                       isCircle ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 },
                       { 68, 135, 220, 255 })) {
            if (!isCircle) {
                PushUndoIfActive();
            }
            collider.shape = Physics::ColliderShape::Circle;
            m_focusedTextFieldId = -1;
        }
        cy += 24.0f;

        // 2. Solid, Static & Trigger (Sensor) Flags
        DrawCheckbox(x, cy, "Solid Collision", collider.isSolid);
        DrawCheckbox(x + w * 0.52f, cy, "Static Body", collider.isStatic);
        cy += 22.0f;

        DrawCheckbox(x, cy, "Trigger (Sensor)", collider.isTrigger);
        if (collider.isTrigger) {
            const char* sensorHint = "[Pass-Through]";
            int hintW = Renderer2D::MeasureTextWidth(sensorHint, 14);
            Renderer2D::DrawLabel(sensorHint, x + w - static_cast<float>(hintW), cy + 2.0f, 14, { 255, 195, 70, 255 });
        }
        cy += 23.0f;

        // 3. Circle Radius Slider + [Auto] Fit Button (shown when Shape == Circle)
        if (collider.shape == Physics::ColliderShape::Circle) {
            bool isAutoRadius = (collider.radius <= 0.5f);
            float displayRadius = collider.GetEffectiveRadius(transform.width, transform.height);
            float prevRadius = displayRadius;

            DrawFloatSlider(x, cy, w - 44.0f, "Radius", displayRadius, 4.0f, 250.0f, baseSliderId + 0);
            if (std::fabs(displayRadius - prevRadius) > 0.01f) {
                collider.radius = std::max(4.0f, std::round(displayRadius));
            }

            if (DrawButton(x + w - 40.0f, cy, 40.0f, 17.0f, "Auto",
                           isAutoRadius ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                           { 64, 128, 210, 255 })) {
                if (!isAutoRadius) {
                    PushUndoIfActive();
                }
                collider.radius = 0.0f;
                m_focusedTextFieldId = -1;
            }
            cy += 21.0f;
        }

        // 4. Bounciness / Restitution Slider (0% = Inelastic, 100% = Elastic Bounce)
        if (!collider.isTrigger) {
            float bouncePct = std::round(collider.restitution * 100.0f);
            DrawFloatSlider(x, cy, w, "Bounce (%)", bouncePct, 0.0f, 100.0f, baseSliderId + 1);
            collider.restitution = std::max(0.0f, std::min(1.0f, std::round(bouncePct) / 100.0f));
            cy += 23.0f;
        }

        return cy - y;
    }

    float EditorGui::DrawCameraControls(float x, float y, float w, Scene::CameraComponent& camera, int baseSliderId) {
        float cy = y;

        // 1. Camera Mode Row: [Fixed] / [Follow] toggle buttons + Primary checkbox
        Renderer2D::DrawLabel("Mode:", x, cy + 3.0f, 14, { 185, 190, 205, 255 });

        bool isFixed  = !camera.followTarget;
        bool isFollow = camera.followTarget;

        if (DrawButton(x + 48.0f, cy, 62.0f, 20.0f, "Fixed",
                       isFixed ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 },
                       { 68, 135, 220, 255 })) {
            if (!isFixed) {
                PushUndoIfActive();
            }
            camera.followTarget = false;
            camera.runtimeInitialized = false;
            m_focusedTextFieldId = -1;
        }

        if (DrawButton(x + 114.0f, cy, 64.0f, 20.0f, "Follow",
                       isFollow ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 },
                       { 68, 135, 220, 255 })) {
            if (!isFollow) {
                PushUndoIfActive();
            }
            camera.followTarget = true;
            camera.runtimeInitialized = false;
            m_focusedTextFieldId = -1;
        }

        DrawCheckbox(x + 190.0f, cy + 1.0f, "Primary", camera.isPrimary);
        cy += 25.0f;

        // 2. Follow Target Controls (Target entity name + [Self] / [Player] presets + Smooth Damping)
        if (camera.followTarget) {
            DrawTextInputBox(x, cy, w - 96.0f, "Target:", camera.targetName, 5);

            bool isSelfTarget = (camera.targetName.empty() || camera.targetName == "Self");
            if (DrawButton(x + w - 92.0f, cy, 40.0f, 23.0f, "Self",
                           isSelfTarget ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                           { 64, 128, 210, 255 })) {
                if (!isSelfTarget) {
                    PushUndoIfActive();
                }
                camera.targetName = "";
                camera.runtimeInitialized = false;
                m_focusedTextFieldId = -1;
            }

            bool isPlayerTarget = (camera.targetName == "Player");
            if (DrawButton(x + w - 48.0f, cy, 48.0f, 23.0f, "Player",
                           isPlayerTarget ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                           { 64, 128, 210, 255 })) {
                if (!isPlayerTarget) {
                    PushUndoIfActive();
                }
                camera.targetName = "Player";
                camera.runtimeInitialized = false;
                m_focusedTextFieldId = -1;
            }
            cy += 27.0f;

            DrawCheckbox(x, cy, "Smooth Follow (Damping)", camera.smoothFollow);
            cy += 22.0f;

            if (camera.smoothFollow) {
                DrawFloatSlider(x, cy, w, "Follow Spd", camera.followSpeed, 1.0f, 25.0f, baseSliderId + 0);
                camera.followSpeed = std::round(camera.followSpeed);
                cy += 21.0f;
            }
        }

        // 3. Zoom & Framing Offset Sliders
        float zoomPct = std::round(camera.zoom * 100.0f);
        DrawFloatSlider(x, cy, w - 42.0f, "Zoom (%)", zoomPct, 25.0f, 400.0f, baseSliderId + 1);
        camera.zoom = std::max(0.25f, std::min(4.00f, std::round(zoomPct) / 100.0f));

        bool isZoom1x = (std::fabs(camera.zoom - 1.0f) < 0.01f);
        if (DrawButton(x + w - 38.0f, cy, 38.0f, 17.0f, "100%",
                       isZoom1x ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                       { 64, 128, 210, 255 })) {
            if (!isZoom1x) {
                PushUndoIfActive();
            }
            camera.zoom = 1.0f;
            m_focusedTextFieldId = -1;
        }
        cy += 21.0f;

        float prevOffX = camera.offsetX;
        float prevOffY = camera.offsetY;
        DrawFloatSlider(x, cy,         w, "Offset X", camera.offsetX, -300.0f, 300.0f, baseSliderId + 2);
        DrawFloatSlider(x, cy + 21.0f, w, "Offset Y", camera.offsetY, -300.0f, 300.0f, baseSliderId + 3);
        camera.offsetX = std::round(camera.offsetX);
        camera.offsetY = std::round(camera.offsetY);
        if (std::fabs(camera.offsetX - prevOffX) > 0.01f || std::fabs(camera.offsetY - prevOffY) > 0.01f) {
            camera.runtimeInitialized = false;
        }
        cy += 44.0f;

        return cy - y;
    }

    float EditorGui::DrawAudioControls(float x, float y, float w, Scene::AudioComponent& audio, int baseSliderId) {
        float cy = y;

        const bool hasPath = !audio.audioPath.empty();
        const bool isCurrentlyPlaying = hasPath && (audio.isMusic
            ? AudioSystem::IsMusicPlaying(audio.audioPath)
            : AudioSystem::IsSoundPlaying(audio.audioPath));

        // 1. Quick Test Toolbar Row: [> Test Audio] / [Stop] + Mode Pills [SFX] / [Music]
        float testBtnW = 104.0f;
        float stopBtnW = 48.0f;
        const char* testBtnLabel = isCurrentlyPlaying ? "> Restart Test" : "> Test Audio";
        ColorRGBA testBtnBg = isCurrentlyPlaying ? ColorRGBA{ 38, 145, 88, 255 } : ColorRGBA{ 42, 128, 78, 255 };
        ColorRGBA testBtnHv = isCurrentlyPlaying ? ColorRGBA{ 52, 175, 108, 255 } : ColorRGBA{ 56, 160, 98, 255 };

        if (DrawButton(x, cy, testBtnW, 22.0f, testBtnLabel, testBtnBg, testBtnHv)) {
            if (hasPath) {
                if (audio.isMusic) {
                    AudioSystem::StopSound(audio.audioPath);
                    AudioSystem::PlayMusic(audio.audioPath, audio.loop, audio.volume, audio.pitch, audio.pan);
                } else {
                    AudioSystem::StopMusic(audio.audioPath);
                    AudioSystem::PlaySound(audio.audioPath, audio.volume, audio.pitch, audio.pan);
                }
            }
            m_focusedTextFieldId = -1;
        }

        if (DrawButton(x + testBtnW + 4.0f, cy, stopBtnW, 22.0f, "Stop",
                       isCurrentlyPlaying ? ColorRGBA{ 155, 52, 58, 255 } : ColorRGBA{ 92, 42, 46, 255 },
                       { 182, 68, 75, 255 })) {
            if (hasPath) {
                AudioSystem::StopSound(audio.audioPath);
                AudioSystem::StopMusic(audio.audioPath);
            }
            m_focusedTextFieldId = -1;
        }

        // Mode pills on the right ([SFX] / [Music])
        float modeBtnW = (w - (testBtnW + stopBtnW + 12.0f) - 4.0f) * 0.5f;
        float sfxBtnX  = x + testBtnW + stopBtnW + 12.0f;
        float musBtnX  = sfxBtnX + modeBtnW + 4.0f;

        if (DrawButton(sfxBtnX, cy, modeBtnW, 22.0f, "SFX",
                       !audio.isMusic ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 },
                       { 68, 135, 220, 255 })) {
            if (audio.isMusic) {
                PushUndoIfActive();
                if (hasPath) AudioSystem::StopMusic(audio.audioPath);
            }
            audio.isMusic = false;
            audio.runtimeStarted = false;
            m_focusedTextFieldId = -1;
        }

        if (DrawButton(musBtnX, cy, modeBtnW, 22.0f, "Music",
                       audio.isMusic ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 },
                       { 68, 135, 220, 255 })) {
            if (!audio.isMusic) {
                PushUndoIfActive();
                if (hasPath) AudioSystem::StopSound(audio.audioPath);
            }
            audio.isMusic = true;
            audio.runtimeStarted = false;
            m_focusedTextFieldId = -1;
        }
        cy += 26.0f;

        // 2. Audio Path Text Input (audioPath, fieldId == 7)
        DrawTextInputBox(x, cy, w, "Path:", audio.audioPath, 7);
        cy += 26.0f;

        // 3. Built-in Starter Audio Clip Presets ("Coin", "Jump", "Hit", "Music", "None")
        struct AudioPreset {
            const char* label;
            const char* path;
            bool isMusic;
            bool loop;
        };
        static const AudioPreset AUDIO_PRESETS[5] = {
            { "Coin",  "examples/assets/audio/coin.wav",       false, false },
            { "Jump",  "examples/assets/audio/jump.wav",       false, false },
            { "Hit",   "examples/assets/audio/hit.wav",        false, false },
            { "Music", "examples/assets/audio/music_loop.wav", true,  true  },
            { "None",  "",                                     false, false }
        };

        float presetGap = 4.0f;
        float presetBtnW = (w - presetGap * 4.0f) / 5.0f;
        for (int i = 0; i < 5; ++i) {
            const auto& p = AUDIO_PRESETS[i];
            bool isSelected = (audio.audioPath == p.path);
            ColorRGBA bg = isSelected ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 38, 41, 52, 255 };
            ColorRGBA hv = isSelected ? ColorRGBA{ 68, 135, 220, 255 } : ColorRGBA{ 52, 56, 70, 255 };
            if (DrawButton(x + static_cast<float>(i) * (presetBtnW + presetGap), cy, presetBtnW, 20.0f, p.label, bg, hv)) {
                if (audio.audioPath != p.path || audio.isMusic != p.isMusic) {
                    PushUndoIfActive();
                    if (!audio.audioPath.empty()) {
                        AudioSystem::StopSound(audio.audioPath);
                        AudioSystem::StopMusic(audio.audioPath);
                    }
                    audio.audioPath = p.path;
                    audio.isMusic   = p.isMusic;
                    if (p.isMusic) {
                        audio.loop = true;
                    }
                    audio.runtimeStarted = false;
                }
                if (m_focusedTextFieldId == 7) {
                    ResetTextCaret();
                }
            }
        }
        cy += 25.0f;

        // 4. First-Level AudioSource Flags (playOnStart, loop, isMusic, playOnCollision)
        bool prevLoop    = audio.loop;
        bool prevIsMusic = audio.isMusic;

        DrawCheckbox(x,             cy, "Play On Start", audio.playOnStart);
        DrawCheckbox(x + w * 0.54f, cy, "Loop",          audio.loop);
        cy += 22.0f;

        DrawCheckbox(x,             cy, "Is Music (Stream)", audio.isMusic);
        DrawCheckbox(x + w * 0.54f, cy, "On Collision",      audio.playOnCollision);
        cy += 24.0f;

        if (audio.isMusic != prevIsMusic) {
            if (!audio.audioPath.empty()) {
                AudioSystem::StopSound(audio.audioPath);
                AudioSystem::StopMusic(audio.audioPath);
            }
            audio.runtimeStarted = false;
        } else if (audio.loop != prevLoop && audio.isMusic && !audio.audioPath.empty()) {
            AudioSystem::SetMusicLooping(audio.audioPath, audio.loop);
        }

        // 5. Volume, Pitch & Pan Sliders (per-entity AudioSource parameters)
        float prevClipVol   = audio.volume;
        float prevClipPitch = audio.pitch;
        float prevClipPan   = audio.pan;

        float volPct = std::round(audio.volume * 100.0f);
        DrawFloatSlider(x, cy, w - 42.0f, "Volume (%)", volPct, 0.0f, 100.0f, baseSliderId + 0);
        audio.volume = std::max(0.0f, std::min(1.0f, std::round(volPct) / 100.0f));

        bool isVol100 = (std::fabs(audio.volume - 1.0f) < 0.01f);
        if (DrawButton(x + w - 38.0f, cy, 38.0f, 17.0f, "100%",
                       isVol100 ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                       { 64, 128, 210, 255 })) {
            if (!isVol100) {
                PushUndoIfActive();
            }
            audio.volume = 1.0f;
            m_focusedTextFieldId = -1;
        }
        cy += 21.0f;

        float pitchPct = std::round(audio.pitch * 100.0f);
        DrawFloatSlider(x, cy, w - 42.0f, "Pitch (%)", pitchPct, 25.0f, 250.0f, baseSliderId + 1);
        audio.pitch = std::max(0.1f, std::min(4.0f, std::round(pitchPct) / 100.0f));

        bool isPitch1x = (std::fabs(audio.pitch - 1.0f) < 0.01f);
        if (DrawButton(x + w - 38.0f, cy, 38.0f, 17.0f, "100%",
                       isPitch1x ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                       { 64, 128, 210, 255 })) {
            if (!isPitch1x) {
                PushUndoIfActive();
            }
            audio.pitch = 1.0f;
            m_focusedTextFieldId = -1;
        }
        cy += 21.0f;

        float panPct = std::round(audio.pan * 100.0f);
        DrawFloatSlider(x, cy, w - 42.0f, "Pan (L/R)", panPct, 0.0f, 100.0f, baseSliderId + 2);
        audio.pan = std::max(0.0f, std::min(1.0f, std::round(panPct) / 100.0f));

        bool isPanCenter = (std::fabs(audio.pan - 0.5f) < 0.01f);
        if (DrawButton(x + w - 38.0f, cy, 38.0f, 17.0f, "50%",
                       isPanCenter ? ColorRGBA{ 52, 115, 195, 255 } : ColorRGBA{ 40, 44, 56, 255 },
                       { 64, 128, 210, 255 })) {
            if (!isPanCenter) {
                PushUndoIfActive();
            }
            audio.pan = 0.5f;
            m_focusedTextFieldId = -1;
        }
        cy += 22.0f;

        if (!audio.audioPath.empty()) {
            if (std::fabs(audio.volume - prevClipVol) > 1e-4f) {
                if (audio.isMusic) AudioSystem::SetMusicVolume(audio.audioPath, audio.volume);
                else               AudioSystem::SetSoundVolume(audio.audioPath, audio.volume);
            }
            if (std::fabs(audio.pitch - prevClipPitch) > 1e-4f) {
                if (audio.isMusic) AudioSystem::SetMusicPitch(audio.audioPath, audio.pitch);
                else               AudioSystem::SetSoundPitch(audio.audioPath, audio.pitch);
            }
            if (std::fabs(audio.pan - prevClipPan) > 1e-4f) {
                if (audio.isMusic) AudioSystem::SetMusicPan(audio.audioPath, audio.pan);
                else               AudioSystem::SetSoundPan(audio.audioPath, audio.pan);
            }
        }

        // 6. Project Audio Volume Buses (Master / SFX / Music - persisted to audio_settings.talos)
        Renderer2D::DrawLine2D(x, cy, x + w, cy, { 46, 50, 62, 255 });
        cy += 6.0f;
        Renderer2D::DrawLabel("PROJECT AUDIO BUSES", x, cy + 2.0f, 13, { 130, 190, 245, 255 });

        float prevMaster = AudioSystem::GetMasterVolume();
        float prevSfx    = AudioSystem::GetSFXVolume();
        float prevMusic  = AudioSystem::GetMusicBusVolume();

        bool allDefault = (std::fabs(prevMaster - 1.0f) < 0.01f &&
                           std::fabs(prevSfx - 1.0f) < 0.01f &&
                           std::fabs(prevMusic - 1.0f) < 0.01f);
        if (DrawButton(x + w - 48.0f, cy, 48.0f, 18.0f, "Reset",
                       allDefault ? ColorRGBA{ 38, 41, 52, 255 } : ColorRGBA{ 52, 95, 165, 255 },
                       { 68, 125, 205, 255 })) {
            AudioSystem::ResetDefaultVolumes();
            AudioSystem::SaveConfig();
            m_focusedTextFieldId = -1;
        }
        cy += 22.0f;

        float masterPct = std::round(AudioSystem::GetMasterVolume() * 100.0f);
        DrawFloatSlider(x, cy, w, "Master (%)", masterPct, 0.0f, 100.0f, baseSliderId + 3);
        float newMaster = std::max(0.0f, std::min(1.0f, std::round(masterPct) / 100.0f));
        cy += 21.0f;

        float sfxPct = std::round(AudioSystem::GetSFXVolume() * 100.0f);
        DrawFloatSlider(x, cy, w, "SFX (%)", sfxPct, 0.0f, 100.0f, baseSliderId + 4);
        float newSfx = std::max(0.0f, std::min(1.0f, std::round(sfxPct) / 100.0f));
        cy += 21.0f;

        float musicPct = std::round(AudioSystem::GetMusicBusVolume() * 100.0f);
        DrawFloatSlider(x, cy, w, "Music (%)", musicPct, 0.0f, 100.0f, baseSliderId + 5);
        float newMusic = std::max(0.0f, std::min(1.0f, std::round(musicPct) / 100.0f));
        cy += 22.0f;

        if (std::fabs(newMaster - prevMaster) > 1e-4f ||
            std::fabs(newSfx - prevSfx) > 1e-4f ||
            std::fabs(newMusic - prevMusic) > 1e-4f) {
            AudioSystem::SetMasterVolume(newMaster);
            AudioSystem::SetSFXVolume(newSfx);
            AudioSystem::SetMusicBusVolume(newMusic);
            AudioSystem::SaveConfig();
        }

        return cy - y;
    }

}

