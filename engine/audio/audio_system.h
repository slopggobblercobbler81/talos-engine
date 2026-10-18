// Audio subsystem managing device lifecycle, in-memory Sound effects, and Music streaming
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#ifdef PlaySound
#undef PlaySound
#endif

namespace Talos::Audio {

    enum class AudioFormat : std::uint8_t {
        Unknown = 0,
        WAV,
        OGG,
        MP3
    };

    struct SoundInfo {
        AudioFormat format = AudioFormat::Unknown;
        std::uint32_t frameCount = 0;
        std::uint32_t sampleRate = 0;
        std::uint32_t sampleSize = 0;
        std::uint32_t channels = 0;
        float durationSeconds = 0.0f;
        std::size_t memoryBytes = 0;
    };

    struct MusicInfo {
        AudioFormat format = AudioFormat::Unknown;
        std::uint32_t frameCount = 0;
        std::uint32_t sampleRate = 0;
        std::uint32_t sampleSize = 0;
        std::uint32_t channels = 0;
        float durationSeconds = 0.0f;
        std::size_t streamBufferBytes = 0;
    };

    class AudioSystem {
    public:
        // --- Device Lifecycle ---
        static bool Init();
        static void Shutdown();
        static bool IsInitialized();
        static bool IsDeviceReady();

        // Feeds active streaming Music buffers; call once per frame in the main loop
        static void Update();

        // --- Volume Bus Controls (Project & Script-Ready API: 0.0f = Mute, 1.0f = 100%) ---
        static void SetMasterVolume(float volume);
        static float GetMasterVolume();

        static void SetSFXVolume(float volume);
        static float GetSFXVolume();

        static void SetMusicVolume(float volume);
        static float GetMusicVolume();
        static void SetMusicBusVolume(float volume);
        static float GetMusicBusVolume();

        static void ResetDefaultVolumes();
        static bool SaveConfig(const std::string& filePath = "examples/scenes/audio_settings.talos");
        static bool LoadConfig(const std::string& filePath = "examples/scenes/audio_settings.talos");

        // --- Format Detection & Validation ---
        static AudioFormat DetectFormat(const std::string& path);
        static bool IsSupportedSoundFormat(const std::string& path); // WAV, OGG
        static bool IsSupportedMusicFormat(const std::string& path); // OGG, MP3, WAV

        // --- Sound Effects (In-Memory: WAV / OGG) ---
        static bool LoadSound(const std::string& path);
        static void UnloadSound(const std::string& path);
        static void UnloadAllSounds();
        static bool HasSound(const std::string& path);

        static bool PlaySound(const std::string& path,
                              float volume = 1.0f,
                              float pitch = 1.0f,
                              float pan = 0.5f);
        static void StopSound(const std::string& path);
        static void PauseSound(const std::string& path);
        static void ResumeSound(const std::string& path);
        static void StopAllSounds();

        static bool IsSoundPlaying(const std::string& path);
        static bool IsSoundPaused(const std::string& path);

        static void SetSoundVolume(const std::string& path, float volume);
        static float GetSoundVolume(const std::string& path);
        static float GetEffectiveSoundVolume(const std::string& path);
        static void SetSoundPitch(const std::string& path, float pitch);
        static float GetSoundPitch(const std::string& path);
        static void SetSoundPan(const std::string& path, float pan);
        static float GetSoundPan(const std::string& path);

        static SoundInfo GetSoundInfo(const std::string& path);

        // --- Music Tracks (Streaming: OGG / MP3 / WAV) ---
        static bool LoadMusic(const std::string& path);
        static void UnloadMusic(const std::string& path);
        static void UnloadAllMusic();
        static bool HasMusic(const std::string& path);

        static bool PlayMusic(const std::string& path,
                              bool loop = true,
                              float volume = 1.0f,
                              float pitch = 1.0f,
                              float pan = 0.5f);
        static void StopMusic(const std::string& path);
        static void PauseMusic(const std::string& path);
        static void ResumeMusic(const std::string& path);
        static void StopAllMusic();

        static bool IsMusicPlaying(const std::string& path);
        static bool IsMusicPaused(const std::string& path);

        static void SeekMusic(const std::string& path, float positionSeconds);
        static void SetMusicLooping(const std::string& path, bool loop);
        static bool IsMusicLooping(const std::string& path);

        static void SetMusicVolume(const std::string& path, float volume);
        static float GetMusicVolume(const std::string& path);
        static float GetEffectiveMusicVolume(const std::string& path);
        static void SetMusicPitch(const std::string& path, float pitch);
        static float GetMusicPitch(const std::string& path);
        static void SetMusicPan(const std::string& path, float pan);
        static float GetMusicPan(const std::string& path);

        static float GetMusicTimeLength(const std::string& path);
        static float GetMusicTimePlayed(const std::string& path);
        static MusicInfo GetMusicInfo(const std::string& path);

        // --- Diagnostics & Telemetry ---
        static std::size_t GetCachedSoundCount();
        static std::size_t GetCachedMusicCount();
        static std::size_t GetActiveMusicStreamCount();
        static std::size_t GetTotalSoundMemoryBytes();
        static std::size_t GetTotalMusicMemoryBytes();
    };

} // namespace Talos::Audio
