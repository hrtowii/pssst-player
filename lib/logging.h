#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#define DEBUG

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERR
} log_level_t;

extern log_level_t g_log_level;

void set_log_level(log_level_t level);
log_level_t get_log_level(void);

bool log_init(const char *path);
void log_shutdown(void);

void log_write(log_level_t level, const char *level_str, const char *tag,
               const char *file, int line, const char *fmt, ...);

#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#ifdef DEBUG
#define LOG_DEBUG(tag, fmt, ...) \
    log_write(LOG_LEVEL_DEBUG, "DEBUG", tag, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(tag, fmt, ...) ((void)0)
#endif

#define LOG_INFO(tag, fmt, ...) \
    log_write(LOG_LEVEL_INFO, "INFO", tag, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...) \
    log_write(LOG_LEVEL_WARN, "WARN", tag, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERR(tag, fmt, ...) \
    log_write(LOG_LEVEL_ERR, "ERR", tag, __FILENAME__, __LINE__, fmt, ##__VA_ARGS__)

#endif
