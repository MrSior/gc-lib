#ifndef GC_PROJECT_LOG_H
#define GC_PROJECT_LOG_H

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <array>

enum class LOG_LEVEL {
    CRITICAL,
    WARNING,
    INFO,
    DEBUG,
};

#define LOG_LVL LOG_LEVEL::DEBUG

void LOG_PRINTF(const char *format, ...);

extern const char* log_level_string[];

#define LOG(level, fmt, ...)                                                                                                            \
    do                                                                                                                                  \
    {                                                                                                                                   \
        if constexpr (level <= LOG_LVL)                                                                                                 \
        {                                                                                                                               \
            fprintf(stderr, "[%s] %s:%d: " fmt "\n", log_level_string[static_cast<size_t>(level)], __FILE__, __LINE__, __VA_ARGS__);    \
        }                                                                                                                               \
    } while (0);                                                                                                                        \
    
#define LOG_CRITICAL(fmt, ...) LOG(LOG_LEVEL::CRITICAL, fmt, __VA_ARGS__)
#define LOG_WARNING(fmt, ...) LOG(LOG_LEVEL::WARNING, fmt, __VA_ARGS__)
#define LOG_INFO(fmt, ...) LOG(LOG_LEVEL::INFO, fmt, __VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG(LOG_LEVEL::DEBUG, fmt, __VA_ARGS__)

#endif //GC_PROJECT_LOG_H