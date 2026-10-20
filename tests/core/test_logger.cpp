// Unit tests for Logger: severity classification, path normalization, and ring buffer capacity
#include "tests/test_framework.h"
#include "core/logger.h"
#include <string>

using namespace Talos::Core;

TEST(Clear_ResetsEntriesAndLevelCounts) {
    Logger::Clear();
    TALOS_LOG_INFO("Initial info");
    TALOS_LOG_WARN("Initial warning");
    TALOS_LOG_ERROR("Initial error");
    CHECK_EQ(Logger::GetEntries().size(), static_cast<size_t>(3));

    Logger::Clear();
    CHECK(Logger::GetEntries().empty());
    CHECK_EQ(Logger::GetCountByLevel(LogLevel::Info), 0);
    CHECK_EQ(Logger::GetCountByLevel(LogLevel::Warning), 0);
    CHECK_EQ(Logger::GetCountByLevel(LogLevel::Error), 0);
}

TEST(LevelToString_ReturnsCanonicalTags) {
    CHECK_EQ( std::string(Logger::LevelToString(LogLevel::Info)),    std::string("INFO") );
    CHECK_EQ( std::string(Logger::LevelToString(LogLevel::Warning)), std::string("WARN") );
    CHECK_EQ( std::string(Logger::LevelToString(LogLevel::Error)),   std::string("ERROR") );
}

TEST(WriteAndMacros_RecordSeverityMessageLineAndTimestamp) {
    Logger::Clear();

    TALOS_LOG_INFO("Engine initialized");
    TALOS_LOG_WARN("Fallback texture used");
    TALOS_LOG_ERROR("Failed to open scene");
    Logger::Write(LogLevel::Info, "Explicit write call", "core/window.cpp", 42);

    const auto& entries = Logger::GetEntries();
    CHECK_EQ(entries.size(), static_cast<size_t>(4));
    CHECK_EQ(Logger::GetCountByLevel(LogLevel::Info), 2);
    CHECK_EQ(Logger::GetCountByLevel(LogLevel::Warning), 1);
    CHECK_EQ(Logger::GetCountByLevel(LogLevel::Error), 1);

    CHECK(entries[0].level == LogLevel::Info);
    CHECK_EQ(entries[0].message, std::string("Engine initialized"));
    CHECK(entries[0].line > 0);
    CHECK_EQ(entries[0].timestamp.size(), static_cast<size_t>(8));
    CHECK_EQ(entries[0].timestamp[2], ':');
    CHECK_EQ(entries[0].timestamp[5], ':');

    CHECK(entries[1].level == LogLevel::Warning);
    CHECK_EQ(entries[1].message, std::string("Fallback texture used"));

    CHECK(entries[2].level == LogLevel::Error);
    CHECK_EQ(entries[2].message, std::string("Failed to open scene"));

    CHECK(entries[3].level == LogLevel::Info);
    CHECK_EQ(entries[3].message, std::string("Explicit write call"));
    CHECK_EQ(entries[3].file, std::string("core/window.cpp"));
    CHECK_EQ(entries[3].line, 42);
}

TEST(FilePathNormalization_ExtractsLayerRelativePathsAndFallbacks) {
    Logger::Clear();

    Logger::Write(LogLevel::Info, "m1", "D:\\Proyectos\\talos-engine\\engine\\core\\input_map.cpp", 10);
    Logger::Write(LogLevel::Info, "m2", "/home/dev/talos-engine/engine/renderer/renderer2d.cpp", 20);
    Logger::Write(LogLevel::Info, "m3", "C:\\repo\\engine\\physics\\physics2d.cpp", 30);
    Logger::Write(LogLevel::Info, "m4", "C:\\repo\\engine\\scene\\scene_serializer.cpp", 40);
    Logger::Write(LogLevel::Info, "m5", "C:\\repo\\editor\\docks\\console_dock.cpp", 50);
    Logger::Write(LogLevel::Info, "m6", "C:\\external\\folder\\standalone_tool.cpp", 60);
    Logger::Write(LogLevel::Info, "m7", "bare_file.cpp", 70);
    Logger::Write(LogLevel::Info, "m8", "", 80);
    Logger::Write(LogLevel::Info, "m9", nullptr, 90);

    const auto& entries = Logger::GetEntries();
    CHECK_EQ(entries.size(), static_cast<size_t>(9));
    CHECK_EQ(entries[0].file, std::string("core/input_map.cpp"));
    CHECK_EQ(entries[1].file, std::string("renderer/renderer2d.cpp"));
    CHECK_EQ(entries[2].file, std::string("physics/physics2d.cpp"));
    CHECK_EQ(entries[3].file, std::string("scene/scene_serializer.cpp"));
    CHECK_EQ(entries[4].file, std::string("editor/docks/console_dock.cpp"));
    CHECK_EQ(entries[5].file, std::string("standalone_tool.cpp"));
    CHECK_EQ(entries[6].file, std::string("bare_file.cpp"));
    CHECK_EQ(entries[7].file, std::string("unknown"));
    CHECK_EQ(entries[8].file, std::string("unknown"));
}

TEST(RingBufferCapacity_CapsAtMaxEntriesAndEvictsOldest) {
    Logger::Clear();

    for (int i = 0; i < 510; ++i) {
        Logger::Write(LogLevel::Info, "msg_" + std::to_string(i), "core/logger.cpp", i);
    }

    const auto& entries = Logger::GetEntries();
    CHECK_EQ(entries.size(), static_cast<size_t>(500));
    CHECK_EQ(entries.front().message, std::string("msg_10"));
    CHECK_EQ(entries.back().message, std::string("msg_509"));
    CHECK_EQ(Logger::GetCountByLevel(LogLevel::Info), 500);

    Logger::Clear();
}

TEST_MAIN()
