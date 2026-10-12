#pragma once

#include <cstdio>
#include <cstdarg>

namespace vpbr {

    enum class LogLevel { Info, Warn, Error, Debug };

    inline void logMessage(LogLevel level, const char* fmt, ...) {
        const char* prefix = "";
        FILE* stream = stdout;

        switch(level) {
            case LogLevel::Info:  prefix = "[INFO ]"; break;
            case LogLevel::Warn:  prefix = "[WARN ]"; break;
            case LogLevel::Error: prefix = "[ERROR]"; stream = stderr; break;
            case LogLevel::Debug: prefix = "[DEBUG]"; break;
        }

        fprintf(stream, "%s ", prefix);

        va_list args;
        va_start(args, fmt);
        vfprintf(stream, fmt, args);
        va_end(args);

        fprintf(stream, "\n");
        fflush(stream);
    }

} // namespace vpbr

#define LOG_INFO(fmt, ...)  vpbr::logMessage(vpbr::LogLevel::Info,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  vpbr::logMessage(vpbr::LogLevel::Warn,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) vpbr::logMessage(vpbr::LogLevel::Error, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) vpbr::logMessage(vpbr::LogLevel::Debug, fmt, ##__VA_ARGS__)