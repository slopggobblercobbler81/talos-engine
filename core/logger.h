#pragma once
#include <string>
#include <vector>

namespace Talos::Core {

    // =========================================================================
    // Diagnostic Log Severity Levels (Layer 1: Core & Platform)
    // =========================================================================
    // Classifies engine, asset, serialization, and editor diagnostic messages
    // into three distinct categories for filtering and visual color-coding.
    enum class LogLevel {
        Info,       // Normal lifecycle events (initialization, asset loads, scene saves)
        Warning,    // Non-fatal anomalies or recoverable fallback states
        Error       // Failures requiring developer attention (missing files, parse errors)
    };

    // =========================================================================
    // Structured Diagnostic Log Entry
    // =========================================================================
    // Stores the message payload alongside the exact C++ source file, line number,
    // and wall-clock timestamp where the diagnostic was emitted.
    struct LogEntry {
        LogLevel level = LogLevel::Info;
        std::string message;
        std::string file;       // Normalized project-relative path (e.g., "renderer/renderer2d.cpp")
        int line = 0;           // 1-indexed source line number (__LINE__)
        std::string timestamp;  // Formatted local time "HH:MM:SS"
    };

    // =========================================================================
    // Centralized Engine Logger (Layer 1: Core & Platform)
    // =========================================================================
    // Provides a static, globally accessible diagnostic log buffer available to all
    // engine layers (Core, Renderer, Physics, Scene, Scripting, and Editor) without
    // violating the unidirectional dependency graph.
    class Logger {
    public:
        // Records a structured log entry with explicit source file and line metadata,
        // mirrors it to standard output, and appends it to the in-memory ring buffer.
        static void Write(LogLevel level, const std::string& message, const char* file, int line);

        // Returns the chronological list of recorded log entries for UI rendering.
        static const std::vector<LogEntry>& GetEntries();

        // Removes all recorded entries from the in-memory log buffer.
        static void Clear();

        // Returns the total number of active entries matching a specific severity level.
        static int GetCountByLevel(LogLevel level);

        // Returns a human-readable short tag for a LogLevel ("INFO", "WARN", "ERROR").
        static const char* LevelToString(LogLevel level);

    private:
        // Strips absolute workspace prefixes and normalizes path separators to '/'
        // so source locations display cleanly (e.g., "scene/scene_serializer.cpp").
        static std::string NormalizeFilePath(const char* rawFilePath);

        // Returns the current local wall-clock time formatted as "HH:MM:SS".
        static std::string GetCurrentTimestamp();
    };

}

// =============================================================================
// Ergonomic Source-Location Logging Macros
// =============================================================================
// Automatically capture the caller's exact __FILE__ and __LINE__ at compile time.
#define TALOS_LOG_INFO(msg)  ::Talos::Core::Logger::Write(::Talos::Core::LogLevel::Info, (msg), __FILE__, __LINE__)
#define TALOS_LOG_WARN(msg)  ::Talos::Core::Logger::Write(::Talos::Core::LogLevel::Warning, (msg), __FILE__, __LINE__)
#define TALOS_LOG_ERROR(msg) ::Talos::Core::Logger::Write(::Talos::Core::LogLevel::Error, (msg), __FILE__, __LINE__)
