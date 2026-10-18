#include "audio/audio_system.h"
#include "core/logger.h"

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

#ifdef PlaySound
#undef PlaySound
#endif

namespace Talos::Audio {

    namespace {

        constexpr std::size_t kMusicStreamSubBufferFrames = 4096u;
        constexpr std::size_t kMusicStreamSubBufferCount = 2u;

        struct CachedSound {
            ::Sound sound{};
            SoundInfo info{};
            bool hasDeviceBuffer = false;
            bool playing = false;
            bool paused = false;
            float volume = 1.0f;
            float pitch = 1.0f;
            float pan = 0.5f;
        };

        struct CachedMusic {
            ::Music music{};
            MusicInfo info{};
            bool hasDeviceStream = false;
            bool playing = false;
            bool paused = false;
            bool looping = true;
            float volume = 1.0f;
            float pitch = 1.0f;
            float pan = 0.5f;
            float simulatedTimePlayed = 0.0f;
        };

        bool s_Initialized = false;
        bool s_DeviceReady = false;
        float s_MasterVolume = 1.0f;
        float s_SfxVolume = 1.0f;
        float s_MusicVolume = 1.0f;

        std::unordered_map<std::string, CachedSound> s_SoundCache;
        std::unordered_map<std::string, CachedMusic> s_MusicCache;

        std::size_t s_TotalSoundMemoryBytes = 0;
        std::size_t s_TotalMusicMemoryBytes = 0;

        float ClampVolume(float v) { return std::clamp(v, 0.0f, 1.0f); }
        float ClampPitch(float p) { return std::clamp(p, 0.05f, 4.0f); }
        float ClampPan(float p) { return std::clamp(p, 0.0f, 1.0f); }
        float ToRaylibPan(float p) { return 1.0f - ClampPan(p); }

        std::string TrimStr(const std::string& s) {
            const auto first = s.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return "";
            const auto last = s.find_last_not_of(" \t\r\n");
            return s.substr(first, last - first + 1);
        }

        void WriteLe16(std::ofstream& out, std::uint16_t v) {
            const char b[2] = { static_cast<char>(v & 0xFFu), static_cast<char>((v >> 8u) & 0xFFu) };
            out.write(b, 2);
        }

        void WriteLe32(std::ofstream& out, std::uint32_t v) {
            const char b[4] = {
                static_cast<char>(v & 0xFFu), static_cast<char>((v >> 8u) & 0xFFu),
                static_cast<char>((v >> 16u) & 0xFFu), static_cast<char>((v >> 24u) & 0xFFu)
            };
            out.write(b, 4);
        }

        bool WritePcm16MonoWav(const std::string& path, std::uint32_t sampleRate, const std::vector<std::int16_t>& samples) {
            std::ofstream out(path, std::ios::binary);
            if (!out.is_open()) return false;
            const std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
            out.write("RIFF", 4); WriteLe32(out, 36u + dataBytes); out.write("WAVE", 4);
            out.write("fmt ", 4); WriteLe32(out, 16u); WriteLe16(out, 1u); WriteLe16(out, 1u);
            WriteLe32(out, sampleRate); WriteLe32(out, sampleRate * 2u); WriteLe16(out, 2u); WriteLe16(out, 16u);
            out.write("data", 4); WriteLe32(out, dataBytes);
            for (std::int16_t s : samples) WriteLe16(out, static_cast<std::uint16_t>(s));
            return out.good();
        }

        void EnsureStarterAudioAssetsExist() {
            const std::string dir = "examples/assets/audio";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            constexpr std::uint32_t kRate = 22050;
            constexpr float kTwoPi = 6.28318530718f;

            if (const std::string p = dir + "/coin.wav"; !std::filesystem::exists(p)) {
                std::vector<std::int16_t> buf(kRate / 5);
                for (std::size_t i = 0; i < buf.size(); ++i) {
                    float t = static_cast<float>(i) / kRate;
                    float f = (i < buf.size() / 3) ? 987.77f : 1318.51f;
                    float env = 1.0f - (static_cast<float>(i) / buf.size());
                    buf[i] = static_cast<std::int16_t>(std::sin(kTwoPi * f * t) * env * 0.45f * 32767.0f);
                }
                WritePcm16MonoWav(p, kRate, buf);
            }

            if (const std::string p = dir + "/jump.wav"; !std::filesystem::exists(p)) {
                std::vector<std::int16_t> buf(kRate / 6);
                float phase = 0.0f;
                for (std::size_t i = 0; i < buf.size(); ++i) {
                    float prog = static_cast<float>(i) / buf.size();
                    phase += (kTwoPi * (220.0f + 440.0f * prog)) / kRate;
                    buf[i] = static_cast<std::int16_t>(std::sin(phase) * (1.0f - prog * 0.8f) * 0.4f * 32767.0f);
                }
                WritePcm16MonoWav(p, kRate, buf);
            }

            if (const std::string p = dir + "/hit.wav"; !std::filesystem::exists(p)) {
                std::vector<std::int16_t> buf(kRate / 8);
                float phase = 0.0f;
                for (std::size_t i = 0; i < buf.size(); ++i) {
                    float prog = static_cast<float>(i) / buf.size();
                    phase += (kTwoPi * 180.0f * (1.0f - 0.65f * prog)) / kRate;
                    float sq = (std::sin(phase) >= 0.0f) ? 1.0f : -1.0f;
                    buf[i] = static_cast<std::int16_t>(sq * (1.0f - prog) * (1.0f - prog) * 0.35f * 32767.0f);
                }
                WritePcm16MonoWav(p, kRate, buf);
            }

            if (const std::string p = dir + "/music_loop.wav"; !std::filesystem::exists(p)) {
                std::vector<std::int16_t> buf(kRate * 2);
                const float notes[4] = { 261.63f, 329.63f, 392.00f, 523.25f };
                const std::size_t step = buf.size() / 4;
                for (std::size_t i = 0; i < buf.size(); ++i) {
                    float env = std::sin((static_cast<float>(i % step) / step) * 3.14159265f);
                    float val = std::sin(kTwoPi * notes[std::min<std::size_t>(3, i / step)] * (static_cast<float>(i) / kRate));
                    buf[i] = static_cast<std::int16_t>(val * env * 0.25f * 32767.0f);
                }
                WritePcm16MonoWav(p, kRate, buf);
            }
        }

    } // namespace

    // --- Device Lifecycle ---

    bool AudioSystem::Init() {
        if (s_Initialized) return s_DeviceReady;
        EnsureStarterAudioAssetsExist();

        ::SetTraceLogLevel(LOG_WARNING);
        if (!::IsAudioDeviceReady()) {
            ::InitAudioDevice();
        }

        s_DeviceReady = ::IsAudioDeviceReady();
        s_Initialized = true;

        if (s_DeviceReady) {
            ::SetMasterVolume(s_MasterVolume);
            TALOS_LOG_INFO("Audio device initialized successfully");
        } else {
            TALOS_LOG_WARN("Hardware audio device unavailable; running in silent fallback mode");
        }
        return s_DeviceReady;
    }

    void AudioSystem::Shutdown() {
        UnloadAllSounds();
        UnloadAllMusic();
        if (s_DeviceReady && ::IsAudioDeviceReady()) {
            ::CloseAudioDevice();
        }
        s_DeviceReady = false;
        s_Initialized = false;
        TALOS_LOG_INFO("Audio subsystem shut down");
    }

    bool AudioSystem::IsInitialized() { return s_Initialized; }
    bool AudioSystem::IsDeviceReady() { return s_DeviceReady && ::IsAudioDeviceReady(); }

    void AudioSystem::Update() {
        if (!s_Initialized) return;
        const bool hwReady = IsDeviceReady();
        for (auto& [path, entry] : s_MusicCache) {
            if (!entry.playing || entry.paused) continue;
            if (hwReady && entry.hasDeviceStream) {
                ::UpdateMusicStream(entry.music);
                if (!::IsMusicStreamPlaying(entry.music)) {
                    entry.playing = false;
                }
            } else {
                entry.simulatedTimePlayed += (1.0f / 60.0f) * entry.pitch;
                if (entry.info.durationSeconds > 0.0f && entry.simulatedTimePlayed >= entry.info.durationSeconds) {
                    if (entry.looping) {
                        entry.simulatedTimePlayed = std::fmod(entry.simulatedTimePlayed, entry.info.durationSeconds);
                    } else {
                        entry.simulatedTimePlayed = 0.0f;
                        entry.playing = false;
                    }
                }
            }
        }
    }

    // --- Volume Bus Controls (Project & Script-Ready API) ---

    void AudioSystem::SetMasterVolume(float volume) {
        s_MasterVolume = ClampVolume(volume);
        if (IsDeviceReady()) ::SetMasterVolume(s_MasterVolume);
    }

    float AudioSystem::GetMasterVolume() { return s_MasterVolume; }

    void AudioSystem::SetSFXVolume(float volume) {
        s_SfxVolume = ClampVolume(volume);
        if (IsDeviceReady()) {
            for (auto& [path, entry] : s_SoundCache) {
                if (entry.hasDeviceBuffer) {
                    ::SetSoundVolume(entry.sound, entry.volume * s_SfxVolume);
                }
            }
        }
    }

    float AudioSystem::GetSFXVolume() { return s_SfxVolume; }

    void AudioSystem::SetMusicVolume(float volume) {
        s_MusicVolume = ClampVolume(volume);
        if (IsDeviceReady()) {
            for (auto& [path, entry] : s_MusicCache) {
                if (entry.hasDeviceStream) {
                    ::SetMusicVolume(entry.music, entry.volume * s_MusicVolume);
                }
            }
        }
    }

    float AudioSystem::GetMusicVolume() { return s_MusicVolume; }

    void AudioSystem::SetMusicBusVolume(float volume) { SetMusicVolume(volume); }
    float AudioSystem::GetMusicBusVolume() { return GetMusicVolume(); }

    void AudioSystem::ResetDefaultVolumes() {
        SetMasterVolume(1.0f);
        SetSFXVolume(1.0f);
        SetMusicVolume(1.0f);
    }

    bool AudioSystem::SaveConfig(const std::string& filePath) {
        if (filePath.empty()) return false;
        std::error_code ec;
        std::filesystem::path p(filePath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path(), ec);
        }

        std::ofstream out(filePath, std::ios::trunc);
        if (!out.is_open()) {
            TALOS_LOG_WARN("AudioSystem::SaveConfig failed to open '" + filePath + "'");
            return false;
        }

        out << "# Talos Engine - Project Audio Volume Configuration\n";
        out << "# Values range from 0.00 (Mute) to 1.00 (100%)\n\n";
        out << "[Audio]\n";
        out << "masterVolume = " << s_MasterVolume << "\n";
        out << "sfxVolume = " << s_SfxVolume << "\n";
        out << "musicVolume = " << s_MusicVolume << "\n";
        return out.good();
    }

    bool AudioSystem::LoadConfig(const std::string& filePath) {
        if (filePath.empty()) return false;
        std::ifstream in(filePath);
        if (!in.is_open()) return false;

        float master = s_MasterVolume;
        float sfx    = s_SfxVolume;
        float music  = s_MusicVolume;
        bool parsedAny = false;

        std::string line;
        while (std::getline(in, line)) {
            std::string trimmed = TrimStr(line);
            if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == '[') continue;
            auto eqPos = trimmed.find('=');
            if (eqPos == std::string::npos) continue;

            std::string key = TrimStr(trimmed.substr(0, eqPos));
            std::string valStr = TrimStr(trimmed.substr(eqPos + 1));
            if (key.empty() || valStr.empty()) continue;

            try {
                float val = std::stof(valStr);
                if (key == "masterVolume" || key == "master_volume") {
                    master = ClampVolume(val);
                    parsedAny = true;
                } else if (key == "sfxVolume" || key == "sfx_volume") {
                    sfx = ClampVolume(val);
                    parsedAny = true;
                } else if (key == "musicVolume" || key == "music_volume") {
                    music = ClampVolume(val);
                    parsedAny = true;
                }
            } catch (...) {
                continue;
            }
        }

        if (!parsedAny) return false;
        SetMasterVolume(master);
        SetSFXVolume(sfx);
        SetMusicVolume(music);
        return true;
    }

    // --- Format Detection & Validation ---

    AudioFormat AudioSystem::DetectFormat(const std::string& path) {
        if (path.empty()) return AudioFormat::Unknown;
        std::string ext = std::filesystem::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (ext == ".wav") return AudioFormat::WAV;
        if (ext == ".ogg") return AudioFormat::OGG;
        if (ext == ".mp3") return AudioFormat::MP3;
        return AudioFormat::Unknown;
    }

    bool AudioSystem::IsSupportedSoundFormat(const std::string& path) {
        const AudioFormat fmt = DetectFormat(path);
        return fmt == AudioFormat::WAV || fmt == AudioFormat::OGG;
    }

    bool AudioSystem::IsSupportedMusicFormat(const std::string& path) {
        const AudioFormat fmt = DetectFormat(path);
        return fmt == AudioFormat::OGG || fmt == AudioFormat::MP3 || fmt == AudioFormat::WAV;
    }

    // --- Sound Effects (In-Memory: WAV / OGG) ---

    bool AudioSystem::LoadSound(const std::string& path) {
        if (path.empty()) return false;
        if (s_SoundCache.find(path) != s_SoundCache.end()) return true;

        if (!IsSupportedSoundFormat(path)) {
            TALOS_LOG_WARN("Unsupported Sound format for '" + path + "' (supported Sound formats: WAV, OGG)");
            return false;
        }

        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || std::filesystem::is_directory(path, ec)) {
            TALOS_LOG_WARN("Sound file not found: " + path);
            return false;
        }

        ::Wave wave = ::LoadWave(path.c_str());
        if (!::IsWaveValid(wave) || wave.frameCount == 0 || wave.sampleRate == 0 || wave.channels == 0) {
            if (wave.data != nullptr) ::UnloadWave(wave);
            TALOS_LOG_WARN("Failed to decode Sound wave data: " + path);
            return false;
        }

        CachedSound entry{};
        entry.info.format = DetectFormat(path);
        entry.info.frameCount = wave.frameCount;
        entry.info.sampleRate = wave.sampleRate;
        entry.info.sampleSize = wave.sampleSize;
        entry.info.channels = wave.channels;
        entry.info.durationSeconds = static_cast<float>(wave.frameCount) / static_cast<float>(wave.sampleRate);
        entry.info.memoryBytes = static_cast<std::size_t>(wave.frameCount) *
                                 static_cast<std::size_t>(wave.channels) *
                                 static_cast<std::size_t>(wave.sampleSize / 8u);

        if (IsDeviceReady()) {
            entry.sound = ::LoadSoundFromWave(wave);
            ::UnloadWave(wave);
            if (!::IsSoundValid(entry.sound)) {
                TALOS_LOG_WARN("Failed to create hardware Sound buffer: " + path);
                return false;
            }
            entry.hasDeviceBuffer = true;
        } else {
            ::UnloadWave(wave);
            entry.hasDeviceBuffer = false;
        }

        s_TotalSoundMemoryBytes += entry.info.memoryBytes;
        s_SoundCache.emplace(path, entry);
        TALOS_LOG_INFO("Loaded Sound in memory: " + path + " (" + std::to_string(entry.info.memoryBytes / 1024) + " KB)");
        return true;
    }

    void AudioSystem::UnloadSound(const std::string& path) {
        auto it = s_SoundCache.find(path);
        if (it == s_SoundCache.end()) return;
        if (it->second.hasDeviceBuffer && IsDeviceReady()) {
            if (it->second.paused) ::ResumeSound(it->second.sound);
            ::StopSound(it->second.sound);
            ::UnloadSound(it->second.sound);
        }
        s_TotalSoundMemoryBytes = (s_TotalSoundMemoryBytes >= it->second.info.memoryBytes)
                                      ? (s_TotalSoundMemoryBytes - it->second.info.memoryBytes) : 0;
        s_SoundCache.erase(it);
    }

    void AudioSystem::UnloadAllSounds() {
        const bool hwReady = IsDeviceReady();
        for (auto& [path, entry] : s_SoundCache) {
            if (entry.hasDeviceBuffer && hwReady) {
                if (entry.paused) ::ResumeSound(entry.sound);
                ::StopSound(entry.sound);
                ::UnloadSound(entry.sound);
            }
        }
        s_SoundCache.clear();
        s_TotalSoundMemoryBytes = 0;
    }

    bool AudioSystem::HasSound(const std::string& path) {
        return s_SoundCache.find(path) != s_SoundCache.end();
    }

    bool AudioSystem::PlaySound(const std::string& path, float volume, float pitch, float pan) {
        if (!HasSound(path) && !LoadSound(path)) return false;
        auto it = s_SoundCache.find(path);
        if (it == s_SoundCache.end()) return false;

        CachedSound& entry = it->second;
        entry.volume = ClampVolume(volume);
        entry.pitch = ClampPitch(pitch);
        entry.pan = ClampPan(pan);
        entry.playing = true;
        entry.paused = false;

        if (entry.hasDeviceBuffer && IsDeviceReady()) {
            ::SetSoundVolume(entry.sound, entry.volume * s_SfxVolume);
            ::SetSoundPitch(entry.sound, entry.pitch);
            ::SetSoundPan(entry.sound, ToRaylibPan(entry.pan));
            ::PlaySound(entry.sound);
        }
        return true;
    }

    void AudioSystem::StopSound(const std::string& path) {
        auto it = s_SoundCache.find(path);
        if (it == s_SoundCache.end()) return;
        CachedSound& entry = it->second;
        if (entry.hasDeviceBuffer && IsDeviceReady()) {
            if (entry.paused) ::ResumeSound(entry.sound);
            ::StopSound(entry.sound);
        }
        entry.playing = false;
        entry.paused = false;
    }

    void AudioSystem::PauseSound(const std::string& path) {
        auto it = s_SoundCache.find(path);
        if (it == s_SoundCache.end() || !it->second.playing || it->second.paused) return;
        if (it->second.hasDeviceBuffer && IsDeviceReady()) ::PauseSound(it->second.sound);
        it->second.paused = true;
    }

    void AudioSystem::ResumeSound(const std::string& path) {
        auto it = s_SoundCache.find(path);
        if (it == s_SoundCache.end() || !it->second.paused) return;
        if (it->second.hasDeviceBuffer && IsDeviceReady()) ::ResumeSound(it->second.sound);
        it->second.paused = false;
        it->second.playing = true;
    }

    void AudioSystem::StopAllSounds() {
        const bool hwReady = IsDeviceReady();
        for (auto& [path, entry] : s_SoundCache) {
            if (entry.hasDeviceBuffer && hwReady) {
                if (entry.paused) ::ResumeSound(entry.sound);
                ::StopSound(entry.sound);
            }
            entry.playing = false;
            entry.paused = false;
        }
    }

    bool AudioSystem::IsSoundPlaying(const std::string& path) {
        auto it = s_SoundCache.find(path);
        if (it == s_SoundCache.end() || it->second.paused) return false;
        if (it->second.hasDeviceBuffer && IsDeviceReady()) {
            it->second.playing = ::IsSoundPlaying(it->second.sound);
        }
        return it->second.playing;
    }

    bool AudioSystem::IsSoundPaused(const std::string& path) {
        auto it = s_SoundCache.find(path);
        return (it != s_SoundCache.end()) ? it->second.paused : false;
    }

    void AudioSystem::SetSoundVolume(const std::string& path, float volume) {
        auto it = s_SoundCache.find(path);
        if (it == s_SoundCache.end()) return;
        it->second.volume = ClampVolume(volume);
        if (it->second.hasDeviceBuffer && IsDeviceReady()) {
            ::SetSoundVolume(it->second.sound, it->second.volume * s_SfxVolume);
        }
    }

    float AudioSystem::GetSoundVolume(const std::string& path) {
        auto it = s_SoundCache.find(path);
        return (it != s_SoundCache.end()) ? it->second.volume : 0.0f;
    }

    float AudioSystem::GetEffectiveSoundVolume(const std::string& path) {
        auto it = s_SoundCache.find(path);
        return (it != s_SoundCache.end()) ? (it->second.volume * s_SfxVolume * s_MasterVolume) : 0.0f;
    }

    void AudioSystem::SetSoundPitch(const std::string& path, float pitch) {
        auto it = s_SoundCache.find(path);
        if (it == s_SoundCache.end()) return;
        it->second.pitch = ClampPitch(pitch);
        if (it->second.hasDeviceBuffer && IsDeviceReady()) ::SetSoundPitch(it->second.sound, it->second.pitch);
    }

    float AudioSystem::GetSoundPitch(const std::string& path) {
        auto it = s_SoundCache.find(path);
        return (it != s_SoundCache.end()) ? it->second.pitch : 0.0f;
    }

    void AudioSystem::SetSoundPan(const std::string& path, float pan) {
        auto it = s_SoundCache.find(path);
        if (it == s_SoundCache.end()) return;
        it->second.pan = ClampPan(pan);
        if (it->second.hasDeviceBuffer && IsDeviceReady()) ::SetSoundPan(it->second.sound, ToRaylibPan(it->second.pan));
    }

    float AudioSystem::GetSoundPan(const std::string& path) {
        auto it = s_SoundCache.find(path);
        return (it != s_SoundCache.end()) ? it->second.pan : 0.5f;
    }

    SoundInfo AudioSystem::GetSoundInfo(const std::string& path) {
        auto it = s_SoundCache.find(path);
        return (it != s_SoundCache.end()) ? it->second.info : SoundInfo{};
    }

    // --- Music Tracks (Streaming: OGG / MP3 / WAV) ---

    bool AudioSystem::LoadMusic(const std::string& path) {
        if (path.empty()) return false;
        if (s_MusicCache.find(path) != s_MusicCache.end()) return true;

        if (!IsSupportedMusicFormat(path)) {
            TALOS_LOG_WARN("Unsupported Music format for '" + path + "' (supported Music formats: OGG, MP3, WAV)");
            return false;
        }

        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || std::filesystem::is_directory(path, ec)) {
            TALOS_LOG_WARN("Music file not found: " + path);
            return false;
        }

        CachedMusic entry{};
        entry.info.format = DetectFormat(path);

        if (IsDeviceReady()) {
            entry.music = ::LoadMusicStream(path.c_str());
            if (!::IsMusicValid(entry.music) || entry.music.frameCount == 0 ||
                entry.music.stream.sampleRate == 0 || entry.music.stream.channels == 0) {
                if (::IsMusicValid(entry.music)) ::UnloadMusicStream(entry.music);
                TALOS_LOG_WARN("Failed to open Music stream: " + path);
                return false;
            }
            entry.music.looping = true;
            entry.hasDeviceStream = true;
            entry.info.frameCount = entry.music.frameCount;
            entry.info.sampleRate = entry.music.stream.sampleRate;
            entry.info.sampleSize = entry.music.stream.sampleSize;
            entry.info.channels = entry.music.stream.channels;
            entry.info.durationSeconds = ::GetMusicTimeLength(entry.music);
        } else {
            ::Wave wave = ::LoadWave(path.c_str());
            if (!::IsWaveValid(wave) || wave.frameCount == 0 || wave.sampleRate == 0 || wave.channels == 0) {
                if (wave.data != nullptr) ::UnloadWave(wave);
                TALOS_LOG_WARN("Failed to validate Music stream file: " + path);
                return false;
            }
            entry.hasDeviceStream = false;
            entry.info.frameCount = wave.frameCount;
            entry.info.sampleRate = wave.sampleRate;
            entry.info.sampleSize = wave.sampleSize;
            entry.info.channels = wave.channels;
            entry.info.durationSeconds = static_cast<float>(wave.frameCount) / static_cast<float>(wave.sampleRate);
            ::UnloadWave(wave);
        }

        entry.info.streamBufferBytes = kMusicStreamSubBufferFrames * kMusicStreamSubBufferCount *
                                       static_cast<std::size_t>(entry.info.channels) *
                                       static_cast<std::size_t>(entry.info.sampleSize / 8u);

        s_TotalMusicMemoryBytes += entry.info.streamBufferBytes;
        s_MusicCache.emplace(path, entry);
        TALOS_LOG_INFO("Opened Music stream: " + path + " (" + std::to_string(entry.info.durationSeconds) + "s)");
        return true;
    }

    void AudioSystem::UnloadMusic(const std::string& path) {
        auto it = s_MusicCache.find(path);
        if (it == s_MusicCache.end()) return;
        if (it->second.hasDeviceStream && IsDeviceReady()) {
            if (it->second.paused) ::ResumeMusicStream(it->second.music);
            ::StopMusicStream(it->second.music);
            ::UnloadMusicStream(it->second.music);
        }
        s_TotalMusicMemoryBytes = (s_TotalMusicMemoryBytes >= it->second.info.streamBufferBytes)
                                      ? (s_TotalMusicMemoryBytes - it->second.info.streamBufferBytes) : 0;
        s_MusicCache.erase(it);
    }

    void AudioSystem::UnloadAllMusic() {
        const bool hwReady = IsDeviceReady();
        for (auto& [path, entry] : s_MusicCache) {
            if (entry.hasDeviceStream && hwReady) {
                if (entry.paused) ::ResumeMusicStream(entry.music);
                ::StopMusicStream(entry.music);
                ::UnloadMusicStream(entry.music);
            }
        }
        s_MusicCache.clear();
        s_TotalMusicMemoryBytes = 0;
    }

    bool AudioSystem::HasMusic(const std::string& path) {
        return s_MusicCache.find(path) != s_MusicCache.end();
    }

    bool AudioSystem::PlayMusic(const std::string& path, bool loop, float volume, float pitch, float pan) {
        if (!HasMusic(path) && !LoadMusic(path)) return false;
        auto it = s_MusicCache.find(path);
        if (it == s_MusicCache.end()) return false;

        CachedMusic& entry = it->second;
        entry.looping = loop;
        entry.volume = ClampVolume(volume);
        entry.pitch = ClampPitch(pitch);
        entry.pan = ClampPan(pan);
        entry.playing = true;
        entry.paused = false;
        entry.simulatedTimePlayed = 0.0f;

        if (entry.hasDeviceStream && IsDeviceReady()) {
            entry.music.looping = loop;
            ::SetMusicVolume(entry.music, entry.volume * s_MusicVolume);
            ::SetMusicPitch(entry.music, entry.pitch);
            ::SetMusicPan(entry.music, ToRaylibPan(entry.pan));
            ::PlayMusicStream(entry.music);
        }
        return true;
    }

    void AudioSystem::StopMusic(const std::string& path) {
        auto it = s_MusicCache.find(path);
        if (it == s_MusicCache.end()) return;
        CachedMusic& entry = it->second;
        if (entry.hasDeviceStream && IsDeviceReady()) {
            if (entry.paused) ::ResumeMusicStream(entry.music);
            ::StopMusicStream(entry.music);
        }
        entry.playing = false;
        entry.paused = false;
        entry.simulatedTimePlayed = 0.0f;
    }

    void AudioSystem::PauseMusic(const std::string& path) {
        auto it = s_MusicCache.find(path);
        if (it == s_MusicCache.end() || !it->second.playing || it->second.paused) return;
        if (it->second.hasDeviceStream && IsDeviceReady()) ::PauseMusicStream(it->second.music);
        it->second.paused = true;
    }

    void AudioSystem::ResumeMusic(const std::string& path) {
        auto it = s_MusicCache.find(path);
        if (it == s_MusicCache.end() || !it->second.paused) return;
        if (it->second.hasDeviceStream && IsDeviceReady()) ::ResumeMusicStream(it->second.music);
        it->second.paused = false;
        it->second.playing = true;
    }

    void AudioSystem::StopAllMusic() {
        const bool hwReady = IsDeviceReady();
        for (auto& [path, entry] : s_MusicCache) {
            if (entry.hasDeviceStream && hwReady) {
                if (entry.paused) ::ResumeMusicStream(entry.music);
                ::StopMusicStream(entry.music);
            }
            entry.playing = false;
            entry.paused = false;
            entry.simulatedTimePlayed = 0.0f;
        }
    }

    bool AudioSystem::IsMusicPlaying(const std::string& path) {
        auto it = s_MusicCache.find(path);
        if (it == s_MusicCache.end() || it->second.paused) return false;
        if (it->second.hasDeviceStream && IsDeviceReady()) {
            it->second.playing = ::IsMusicStreamPlaying(it->second.music);
        }
        return it->second.playing;
    }

    bool AudioSystem::IsMusicPaused(const std::string& path) {
        auto it = s_MusicCache.find(path);
        return (it != s_MusicCache.end()) ? it->second.paused : false;
    }

    void AudioSystem::SeekMusic(const std::string& path, float positionSeconds) {
        auto it = s_MusicCache.find(path);
        if (it == s_MusicCache.end()) return;
        CachedMusic& entry = it->second;
        const float clampedPos = std::clamp(positionSeconds, 0.0f, std::max(0.0f, entry.info.durationSeconds));
        entry.simulatedTimePlayed = clampedPos;
        if (entry.hasDeviceStream && IsDeviceReady()) ::SeekMusicStream(entry.music, clampedPos);
    }

    void AudioSystem::SetMusicLooping(const std::string& path, bool loop) {
        auto it = s_MusicCache.find(path);
        if (it == s_MusicCache.end()) return;
        it->second.looping = loop;
        if (it->second.hasDeviceStream) it->second.music.looping = loop;
    }

    bool AudioSystem::IsMusicLooping(const std::string& path) {
        auto it = s_MusicCache.find(path);
        return (it != s_MusicCache.end()) ? it->second.looping : false;
    }

    void AudioSystem::SetMusicVolume(const std::string& path, float volume) {
        auto it = s_MusicCache.find(path);
        if (it == s_MusicCache.end()) return;
        it->second.volume = ClampVolume(volume);
        if (it->second.hasDeviceStream && IsDeviceReady()) {
            ::SetMusicVolume(it->second.music, it->second.volume * s_MusicVolume);
        }
    }

    float AudioSystem::GetMusicVolume(const std::string& path) {
        auto it = s_MusicCache.find(path);
        return (it != s_MusicCache.end()) ? it->second.volume : 0.0f;
    }

    float AudioSystem::GetEffectiveMusicVolume(const std::string& path) {
        auto it = s_MusicCache.find(path);
        return (it != s_MusicCache.end()) ? (it->second.volume * s_MusicVolume * s_MasterVolume) : 0.0f;
    }

    void AudioSystem::SetMusicPitch(const std::string& path, float pitch) {
        auto it = s_MusicCache.find(path);
        if (it == s_MusicCache.end()) return;
        it->second.pitch = ClampPitch(pitch);
        if (it->second.hasDeviceStream && IsDeviceReady()) ::SetMusicPitch(it->second.music, it->second.pitch);
    }

    float AudioSystem::GetMusicPitch(const std::string& path) {
        auto it = s_MusicCache.find(path);
        return (it != s_MusicCache.end()) ? it->second.pitch : 0.0f;
    }

    void AudioSystem::SetMusicPan(const std::string& path, float pan) {
        auto it = s_MusicCache.find(path);
        if (it == s_MusicCache.end()) return;
        it->second.pan = ClampPan(pan);
        if (it->second.hasDeviceStream && IsDeviceReady()) ::SetMusicPan(it->second.music, ToRaylibPan(it->second.pan));
    }

    float AudioSystem::GetMusicPan(const std::string& path) {
        auto it = s_MusicCache.find(path);
        return (it != s_MusicCache.end()) ? it->second.pan : 0.5f;
    }

    float AudioSystem::GetMusicTimeLength(const std::string& path) {
        auto it = s_MusicCache.find(path);
        return (it != s_MusicCache.end()) ? it->second.info.durationSeconds : 0.0f;
    }

    float AudioSystem::GetMusicTimePlayed(const std::string& path) {
        auto it = s_MusicCache.find(path);
        if (it == s_MusicCache.end()) return 0.0f;
        if (it->second.hasDeviceStream && IsDeviceReady()) return ::GetMusicTimePlayed(it->second.music);
        return it->second.simulatedTimePlayed;
    }

    MusicInfo AudioSystem::GetMusicInfo(const std::string& path) {
        auto it = s_MusicCache.find(path);
        return (it != s_MusicCache.end()) ? it->second.info : MusicInfo{};
    }

    // --- Diagnostics & Telemetry ---

    std::size_t AudioSystem::GetCachedSoundCount() { return s_SoundCache.size(); }
    std::size_t AudioSystem::GetCachedMusicCount() { return s_MusicCache.size(); }

    std::size_t AudioSystem::GetActiveMusicStreamCount() {
        std::size_t active = 0;
        const bool hwReady = IsDeviceReady();
        for (auto& [path, entry] : s_MusicCache) {
            if (entry.paused) continue;
            if (entry.hasDeviceStream && hwReady) entry.playing = ::IsMusicStreamPlaying(entry.music);
            if (entry.playing) ++active;
        }
        return active;
    }

    std::size_t AudioSystem::GetTotalSoundMemoryBytes() { return s_TotalSoundMemoryBytes; }
    std::size_t AudioSystem::GetTotalMusicMemoryBytes() { return s_TotalMusicMemoryBytes; }

} // namespace Talos::Audio
