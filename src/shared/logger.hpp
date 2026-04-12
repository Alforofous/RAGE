#pragma once

#include <cstdio>

namespace RAGE {
    enum class LogLevel {Error, Warn, Info, Debug};

    inline void log(LogLevel level, const char *msg) {
        const char *prefix[] = { "ERROR", "WARN", "INFO", "DEBUG" };
        fprintf(stderr, "[RAGE %s] %s\n", prefix[static_cast<int>(level)], msg);
    }
}