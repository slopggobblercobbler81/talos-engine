#include "core/logger.h"
#include <cstdio>
#include <ctime>
#include <algorithm>

namespace Talos::Core {

    // Maximum number of entries retained in memory to bound RAM usage during long sessions.
    static constexpr size_t MAX_LOG_ENTRIES = 500;
    static std::vector<LogEntry> s_entries;

    std::string Logger::NormalizeFilePath(const char* rawFilePath) {
        if (!rawFilePath || rawFilePath[0] == '\0') {
            return "unknown";
        }

        std::string path(rawFilePath);

        // Convert Windows backslashes to uniform forward slashes
        for (char& c : path) {
            if (c == '\\') {
                c = '/';
            }
        }

        // Look for known engine layer folders so the displayed path is concise and project-relative
        const char* layerDirectories[] = {
            "core/",
            "renderer/",
            "physics/",
            "scene/",
            "scripting/",
            "editor/",
            "examples/"
        };

        for (const char* dir : layerDirectories) {
            size_t pos = path.rfind(dir);
            if (pos != std::string::npos) {
                return path.substr(pos);
            }
        }

        // Fallback: return only the filename if outside standard layer directories
        size_t lastSlash = path.find_last_of('/');
        if (lastSlash != std::string::npos) {
            return path.substr(lastSlash + 1);
        }
        return path;
    }

    std::string Logger::GetCurrentTimestamp() {
        std::time_t now = std::time(nullptr);
        std::tm localTime = {};
#if defined(_WIN32)
        localtime_s(&localTime, &now);
#else
        localtime_r(&now, &localTime);
#endif
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                      localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
        return std::string(buf);
    }

    const char* Logger::LevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::Info:    return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error:   return "ERROR";
            default:                return "LOG";
        }
    }

    void Logger::Write(LogLevel level, const std::string& message, const char* file, int line) {
        if (s_entries.size() >= MAX_LOG_ENTRIES) {
            s_entries.erase(s_entries.begin());
        }

        LogEntry entry;
        entry.level     = level;
        entry.message   = message;
        entry.file      = NormalizeFilePath(file);
        entry.line      = line;
        entry.timestamp = GetCurrentTimestamp();

        // Mirror every structured log entry to standard output/error for terminal debugging
        FILE* stream = (level == LogLevel::Error) ? stderr : stdout;
        std::fprintf(stream, "[%s] [%s] [%s:%d] %s\n",
                     entry.timestamp.c_str(),
                     LevelToString(entry.level),
                     entry.file.c_str(),
                     entry.line,
                     entry.message.c_str());

        s_entries.push_back( entry );
    }

    const std::vector<LogEntry>& Logger::GetEntries() {
        return s_entries;
    }

    void Logger::Clear() {
        s_entries.clear();
    }

    int Logger::GetCountByLevel(LogLevel level) {
        int count = 0;
        for (const auto& entry : s_entries) {
            if (entry.level == level) {
                ++count;
            }
        }
        return count;
    }

}
