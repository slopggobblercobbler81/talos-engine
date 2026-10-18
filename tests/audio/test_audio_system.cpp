// Unit tests for AudioSystem: device lifecycle, WAV/OGG Sound cache, OGG/MP3/WAV Music streaming
#include "tests/test_framework.h"
#include "audio/audio_system.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace Talos::Audio;
namespace fs = std::filesystem;

namespace {

    fs::path TempDir() {
        fs::path dir = fs::temp_directory_path() / "talos_audio_system_tests";
        fs::create_directories(dir);
        return dir;
    }

    void WriteLe16(std::ofstream& out, std::uint16_t value) {
        const char bytes[2] = {
            static_cast<char>(value & 0xFFu),
            static_cast<char>((value >> 8u) & 0xFFu)
        };
        out.write(bytes, 2);
    }

    void WriteLe32(std::ofstream& out, std::uint32_t value) {
        const char bytes[4] = {
            static_cast<char>(value & 0xFFu),
            static_cast<char>((value >> 8u) & 0xFFu),
            static_cast<char>((value >> 16u) & 0xFFu),
            static_cast<char>((value >> 24u) & 0xFFu)
        };
        out.write(bytes, 4);
    }

    std::string CreateTestWavFile(const std::string& filename,
                                  std::uint32_t sampleRate = 22050,
                                  std::size_t frameCount = 4410) {
        const fs::path path = TempDir() / filename;
        std::ofstream out(path, std::ios::binary | std::ios::trunc);

        const std::uint16_t numChannels = 1;
        const std::uint16_t bitsPerSample = 16;
        const std::uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8u);
        const std::uint16_t blockAlign = numChannels * (bitsPerSample / 8u);
        const std::uint32_t dataBytes = static_cast<std::uint32_t>(frameCount * sizeof(std::int16_t));

        out.write("RIFF", 4);
        WriteLe32(out, 36u + dataBytes);
        out.write("WAVE", 4);
        out.write("fmt ", 4);
        WriteLe32(out, 16u);
        WriteLe16(out, 1u);
        WriteLe16(out, numChannels);
        WriteLe32(out, sampleRate);
        WriteLe32(out, byteRate);
        WriteLe16(out, blockAlign);
        WriteLe16(out, bitsPerSample);
        out.write("data", 4);
        WriteLe32(out, dataBytes);

        for (std::size_t i = 0; i < frameCount; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
            const auto sample = static_cast<std::int16_t>(std::sin(6.2831853f * 440.0f * t) * 8000.0f);
            WriteLe16(out, static_cast<std::uint16_t>(sample));
        }

        out.close();
        return path.string();
    }

    // Generates a valid MPEG-1 Layer 3 (MP3) stream (128 kbps, 44100 Hz, Mono, 417 bytes/frame).
    std::string CreateTestMp3File(const std::string& filename, std::size_t mpegFrames = 24) {
        const fs::path path = TempDir() / filename;
        std::ofstream out(path, std::ios::binary | std::ios::trunc);

        std::vector<std::uint8_t> frame(417, 0u);
        frame[0] = 0xFFu; // Syncword
        frame[1] = 0xFBu; // MPEG-1, Layer 3, no CRC
        frame[2] = 0x90u; // 128 kbps, 44100 Hz, no padding
        frame[3] = 0xC4u; // Mono (Single Channel), original

        for (std::size_t i = 0; i < mpegFrames; ++i) {
            out.write(reinterpret_cast<const char*>(frame.data()),
                      static_cast<std::streamsize>(frame.size()));
        }

        out.close();
        return path.string();
    }

    // Locates examples/assets/audio/blip.ogg by walking up from current_path() and copies it to TempDir.
    std::string GetOrCopyTestOggFile(const std::string& filename) {
        const fs::path dst = TempDir() / filename;
        if (fs::exists(dst)) {
            return dst.string();
        }

        fs::path cur = fs::current_path();
        for (int depth = 0; depth < 6; ++depth) {
            const fs::path candidate = cur / "examples" / "assets" / "audio" / "blip.ogg";
            if (fs::exists(candidate)) {
                std::error_code ec;
                fs::copy_file(candidate, dst, fs::copy_options::overwrite_existing, ec);
                return dst.string();
            }
            if (!cur.has_parent_path() || cur.parent_path() == cur) {
                break;
            }
            cur = cur.parent_path();
        }
        return dst.string();
    }

    std::string CreateCorruptFile(const std::string& filename) {
        const fs::path path = TempDir() / filename;
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "NOT_A_VALID_AUDIO_FILE_HEADER_12345";
        out.close();
        return path.string();
    }

} // namespace

TEST(Lifecycle_InitMasterVolumeAndShutdown) {
    AudioSystem::Shutdown();
    CHECK(!AudioSystem::IsInitialized());

    AudioSystem::Init();
    CHECK(AudioSystem::IsInitialized());

    // Keep volume silent during automated test execution
    AudioSystem::SetMasterVolume(0.0f);
    CHECK_NEAR(AudioSystem::GetMasterVolume(), 0.0f, 1e-5f);

    AudioSystem::SetMasterVolume(0.65f);
    CHECK_NEAR(AudioSystem::GetMasterVolume(), 0.65f, 1e-5f);

    // Clamping checks
    AudioSystem::SetMasterVolume(2.5f);
    CHECK_NEAR(AudioSystem::GetMasterVolume(), 1.0f, 1e-5f);

    AudioSystem::SetMasterVolume(-0.5f);
    CHECK_NEAR(AudioSystem::GetMasterVolume(), 0.0f, 1e-5f);

    AudioSystem::Shutdown();
    CHECK(!AudioSystem::IsInitialized());
}

TEST(Formats_DetectionAndValidationRules) {
    CHECK(AudioSystem::DetectFormat("sfx/jump.wav") == AudioFormat::WAV);
    CHECK(AudioSystem::DetectFormat("sfx/JUMP.WAV") == AudioFormat::WAV);
    CHECK(AudioSystem::DetectFormat("sfx/hit.ogg") == AudioFormat::OGG);
    CHECK(AudioSystem::DetectFormat("music/theme.mp3") == AudioFormat::MP3);
    CHECK(AudioSystem::DetectFormat("music/track.flac") == AudioFormat::Unknown);
    CHECK(AudioSystem::DetectFormat("") == AudioFormat::Unknown);

    // Sound supports in-memory WAV and OGG only
    CHECK(AudioSystem::IsSupportedSoundFormat("coin.wav"));
    CHECK(AudioSystem::IsSupportedSoundFormat("blip.ogg"));
    CHECK(!AudioSystem::IsSupportedSoundFormat("song.mp3"));
    CHECK(!AudioSystem::IsSupportedSoundFormat("notes.txt"));

    // Music supports streaming OGG, MP3, and WAV
    CHECK(AudioSystem::IsSupportedMusicFormat("bgm.ogg"));
    CHECK(AudioSystem::IsSupportedMusicFormat("bgm.mp3"));
    CHECK(AudioSystem::IsSupportedMusicFormat("bgm.wav"));
    CHECK(!AudioSystem::IsSupportedMusicFormat("bgm.flac"));
}

TEST(Sound_LoadsCachesAndPlaysWavAndOggInMemory) {
    AudioSystem::Init();
    AudioSystem::SetMasterVolume(0.0f);
    AudioSystem::UnloadAllSounds();

    const std::string wavPath = CreateTestWavFile("sfx_tone.wav", 22050, 4410);
    const std::string oggPath = GetOrCopyTestOggFile("sfx_blip.ogg");
    const std::string mp3Path = CreateTestMp3File("invalid_for_sound.mp3");

    // 1. WAV Sound load & metadata check
    CHECK(AudioSystem::LoadSound(wavPath));
    CHECK(AudioSystem::HasSound(wavPath));
    CHECK_EQ(AudioSystem::GetCachedSoundCount(), static_cast<std::size_t>(1));

    const SoundInfo wavInfo = AudioSystem::GetSoundInfo(wavPath);
    CHECK(wavInfo.format == AudioFormat::WAV);
    CHECK_EQ(wavInfo.sampleRate, 22050u);
    CHECK_EQ(wavInfo.frameCount, 4410u);
    CHECK_EQ(wavInfo.channels, 1u);
    CHECK_NEAR(wavInfo.durationSeconds, 0.2f, 0.01f);
    CHECK(wavInfo.memoryBytes > 0);

    // 2. OGG Sound load & metadata check
    CHECK(AudioSystem::LoadSound(oggPath));
    CHECK(AudioSystem::HasSound(oggPath));
    CHECK_EQ(AudioSystem::GetCachedSoundCount(), static_cast<std::size_t>(2));

    const SoundInfo oggInfo = AudioSystem::GetSoundInfo(oggPath);
    CHECK(oggInfo.format == AudioFormat::OGG);
    CHECK(oggInfo.frameCount > 0);
    CHECK(oggInfo.sampleRate > 0);
    CHECK(oggInfo.memoryBytes > 0);
    CHECK_EQ(AudioSystem::GetTotalSoundMemoryBytes(), wavInfo.memoryBytes + oggInfo.memoryBytes);

    // 3. MP3 must be rejected as a Sound (streaming Music only)
    CHECK(!AudioSystem::LoadSound(mp3Path));
    CHECK_EQ(AudioSystem::GetCachedSoundCount(), static_cast<std::size_t>(2));

    // 4. Playback, pause, resume, parameters, and stop
    CHECK(AudioSystem::PlaySound(wavPath, 0.75f, 1.25f, 0.2f));
    CHECK_NEAR(AudioSystem::GetSoundVolume(wavPath), 0.75f, 1e-5f);
    CHECK_NEAR(AudioSystem::GetSoundPitch(wavPath), 1.25f, 1e-5f);
    CHECK_NEAR(AudioSystem::GetSoundPan(wavPath), 0.2f, 1e-5f);

    AudioSystem::PauseSound(wavPath);
    CHECK(AudioSystem::IsSoundPaused(wavPath));
    CHECK(!AudioSystem::IsSoundPlaying(wavPath));

    AudioSystem::ResumeSound(wavPath);
    CHECK(!AudioSystem::IsSoundPaused(wavPath));

    AudioSystem::StopSound(wavPath);
    CHECK(!AudioSystem::IsSoundPlaying(wavPath));
    CHECK(!AudioSystem::IsSoundPaused(wavPath));

    // 5. Unload individual and all sounds
    AudioSystem::UnloadSound(wavPath);
    CHECK(!AudioSystem::HasSound(wavPath));
    CHECK_EQ(AudioSystem::GetCachedSoundCount(), static_cast<std::size_t>(1));

    AudioSystem::UnloadAllSounds();
    CHECK_EQ(AudioSystem::GetCachedSoundCount(), static_cast<std::size_t>(0));
    CHECK_EQ(AudioSystem::GetTotalSoundMemoryBytes(), static_cast<std::size_t>(0));

    AudioSystem::Shutdown();
}

TEST(Music_StreamsOggMp3AndWavWithControls) {
    AudioSystem::Init();
    AudioSystem::SetMasterVolume(0.0f);
    AudioSystem::UnloadAllMusic();

    const std::string oggPath = GetOrCopyTestOggFile("music_track.ogg");
    const std::string mp3Path = CreateTestMp3File("music_track.mp3", 30);
    const std::string wavPath = CreateTestWavFile("music_track.wav", 22050, 22050);

    // 1. Load OGG, MP3, and WAV streams
    CHECK(AudioSystem::LoadMusic(oggPath));
    CHECK(AudioSystem::LoadMusic(mp3Path));
    CHECK(AudioSystem::LoadMusic(wavPath));
    CHECK_EQ(AudioSystem::GetCachedMusicCount(), static_cast<std::size_t>(3));

    const MusicInfo oggInfo = AudioSystem::GetMusicInfo(oggPath);
    const MusicInfo mp3Info = AudioSystem::GetMusicInfo(mp3Path);
    const MusicInfo wavInfo = AudioSystem::GetMusicInfo(wavPath);

    CHECK(oggInfo.format == AudioFormat::OGG);
    CHECK(oggInfo.frameCount > 0);
    CHECK(oggInfo.durationSeconds > 0.0f);
    CHECK(oggInfo.streamBufferBytes > 0);

    CHECK(mp3Info.format == AudioFormat::MP3);
    CHECK(mp3Info.frameCount > 0);
    CHECK_EQ(mp3Info.sampleRate, 44100u);
    CHECK(mp3Info.durationSeconds > 0.5f);

    CHECK(wavInfo.format == AudioFormat::WAV);
    CHECK_EQ(wavInfo.frameCount, 22050u);
    CHECK_NEAR(wavInfo.durationSeconds, 1.0f, 0.02f);

    // 2. Streaming playback, Update(), pause/resume, seek, loop, and stop
    CHECK(AudioSystem::PlayMusic(mp3Path, false, 0.8f, 1.1f, 0.6f));
    CHECK(AudioSystem::IsMusicPlaying(mp3Path));
    CHECK(!AudioSystem::IsMusicLooping(mp3Path));
    CHECK_NEAR(AudioSystem::GetMusicVolume(mp3Path), 0.8f, 1e-5f);
    CHECK_NEAR(AudioSystem::GetMusicPitch(mp3Path), 1.1f, 1e-5f);
    CHECK_NEAR(AudioSystem::GetMusicPan(mp3Path), 0.6f, 1e-5f);
    CHECK_EQ(AudioSystem::GetActiveMusicStreamCount(), static_cast<std::size_t>(1));

    // Pump streaming buffer
    AudioSystem::Update();
    CHECK(AudioSystem::IsMusicPlaying(mp3Path));

    AudioSystem::SetMusicLooping(mp3Path, true);
    CHECK(AudioSystem::IsMusicLooping(mp3Path));

    AudioSystem::SeekMusic(mp3Path, 0.25f);
    AudioSystem::Update();

    AudioSystem::PauseMusic(mp3Path);
    CHECK(AudioSystem::IsMusicPaused(mp3Path));
    CHECK(!AudioSystem::IsMusicPlaying(mp3Path));
    CHECK_EQ(AudioSystem::GetActiveMusicStreamCount(), static_cast<std::size_t>(0));

    AudioSystem::ResumeMusic(mp3Path);
    CHECK(!AudioSystem::IsMusicPaused(mp3Path));
    CHECK(AudioSystem::IsMusicPlaying(mp3Path));

    AudioSystem::StopMusic(mp3Path);
    CHECK(!AudioSystem::IsMusicPlaying(mp3Path));
    CHECK(!AudioSystem::IsMusicPaused(mp3Path));

    // 3. Unload
    AudioSystem::UnloadMusic(mp3Path);
    CHECK(!AudioSystem::HasMusic(mp3Path));
    CHECK_EQ(AudioSystem::GetCachedMusicCount(), static_cast<std::size_t>(2));

    AudioSystem::UnloadAllMusic();
    CHECK_EQ(AudioSystem::GetCachedMusicCount(), static_cast<std::size_t>(0));
    CHECK_EQ(AudioSystem::GetTotalMusicMemoryBytes(), static_cast<std::size_t>(0));

    AudioSystem::Shutdown();
}

TEST(ErrorHandling_RejectsMissingAndCorruptFiles) {
    AudioSystem::Init();
    AudioSystem::SetMasterVolume(0.0f);

    const std::string corruptWav = CreateCorruptFile("corrupt.wav");
    const std::string corruptOgg = CreateCorruptFile("corrupt.ogg");
    const std::string corruptMp3 = CreateCorruptFile("corrupt.mp3");

    CHECK(!AudioSystem::LoadSound(""));
    CHECK(!AudioSystem::LoadSound("non_existent_path/missing.wav"));
    CHECK(!AudioSystem::LoadSound(corruptWav));
    CHECK(!AudioSystem::LoadSound(corruptOgg));

    CHECK(!AudioSystem::LoadMusic(""));
    CHECK(!AudioSystem::LoadMusic("non_existent_path/missing.ogg"));
    CHECK(!AudioSystem::LoadMusic(corruptWav));
    CHECK(!AudioSystem::LoadMusic(corruptOgg));
    CHECK(!AudioSystem::LoadMusic(corruptMp3));

    AudioSystem::Shutdown();
}

TEST(VolumeBuses_MasterSfxMusicAndConfigPersistence) {
    AudioSystem::Init();
    AudioSystem::ResetDefaultVolumes();

    CHECK_NEAR(AudioSystem::GetMasterVolume(), 1.0f, 0.001f);
    CHECK_NEAR(AudioSystem::GetSFXVolume(), 1.0f, 0.001f);
    CHECK_NEAR(AudioSystem::GetMusicVolume(), 1.0f, 0.001f);
    CHECK_NEAR(AudioSystem::GetMusicBusVolume(), 1.0f, 0.001f);

    // 1. Clamping & bus volume setters/getters
    AudioSystem::SetMasterVolume(1.5f);
    CHECK_NEAR(AudioSystem::GetMasterVolume(), 1.0f, 0.001f);
    AudioSystem::SetMasterVolume(-0.4f);
    CHECK_NEAR(AudioSystem::GetMasterVolume(), 0.0f, 0.001f);
    AudioSystem::SetMasterVolume(0.5f);
    CHECK_NEAR(AudioSystem::GetMasterVolume(), 0.5f, 0.001f);

    AudioSystem::SetSFXVolume(0.5f);
    CHECK_NEAR(AudioSystem::GetSFXVolume(), 0.5f, 0.001f);

    AudioSystem::SetMusicVolume(0.4f);
    CHECK_NEAR(AudioSystem::GetMusicVolume(), 0.4f, 0.001f);
    CHECK_NEAR(AudioSystem::GetMusicBusVolume(), 0.4f, 0.001f);

    // 2. Effective volume scaling across loaded Sound and Music assets
    const std::string wavPath = CreateTestWavFile("bus_test_sfx.wav");
    const std::string musicPath = CreateTestWavFile("bus_test_music.wav");
    CHECK(AudioSystem::LoadSound(wavPath));
    CHECK(AudioSystem::LoadMusic(musicPath));

    AudioSystem::SetSoundVolume(wavPath, 0.8f);
    AudioSystem::SetMusicVolume(musicPath, 0.75f);

    CHECK_NEAR(AudioSystem::GetSoundVolume(wavPath), 0.8f, 0.001f);
    CHECK_NEAR(AudioSystem::GetMusicVolume(musicPath), 0.75f, 0.001f);

    // Effective volume = clipVolume * busVolume * masterVolume
    CHECK_NEAR(AudioSystem::GetEffectiveSoundVolume(wavPath), 0.8f * 0.5f * 0.5f, 0.001f);
    CHECK_NEAR(AudioSystem::GetEffectiveMusicVolume(musicPath), 0.75f * 0.4f * 0.5f, 0.001f);

    // Updating bus volume dynamically updates effective volumes
    AudioSystem::SetSFXVolume(0.25f);
    AudioSystem::SetMusicBusVolume(0.5f);
    CHECK_NEAR(AudioSystem::GetEffectiveSoundVolume(wavPath), 0.8f * 0.25f * 0.5f, 0.001f);
    CHECK_NEAR(AudioSystem::GetEffectiveMusicVolume(musicPath), 0.75f * 0.5f * 0.5f, 0.001f);

    // 3. Project configuration persistence (.talos)
    AudioSystem::SetMasterVolume(0.85f);
    AudioSystem::SetSFXVolume(0.65f);
    AudioSystem::SetMusicVolume(0.35f);

    const std::string configPath = (TempDir() / "test_audio_settings.talos").string();
    CHECK(AudioSystem::SaveConfig(configPath));

    AudioSystem::ResetDefaultVolumes();
    CHECK_NEAR(AudioSystem::GetMasterVolume(), 1.0f, 0.001f);
    CHECK_NEAR(AudioSystem::GetSFXVolume(), 1.0f, 0.001f);
    CHECK_NEAR(AudioSystem::GetMusicVolume(), 1.0f, 0.001f);

    CHECK(AudioSystem::LoadConfig(configPath));
    CHECK_NEAR(AudioSystem::GetMasterVolume(), 0.85f, 0.001f);
    CHECK_NEAR(AudioSystem::GetSFXVolume(), 0.65f, 0.001f);
    CHECK_NEAR(AudioSystem::GetMusicVolume(), 0.35f, 0.001f);

    AudioSystem::ResetDefaultVolumes();
    AudioSystem::Shutdown();
}

TEST_MAIN()
