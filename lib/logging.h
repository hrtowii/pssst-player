#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <string.h>

#define COLOR_RESET "\033[0m"
#define COLOR_DEBUG "\033[36m" // Cyan
#define COLOR_INFO "\033[32m"  // Green
#define COLOR_WARN "\033[33m"  // Yellow
#define COLOR_ERR "\033[31m"   // Red

typedef enum {
  LOG_LEVEL_DEBUG = 0,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERR
} log_level_t;

extern log_level_t g_log_level;
#define __FILENAME__                                                           \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
// DEBUG logging is only compiled for debug builds
// and it is not available to switch to at runtime for release builds
#ifdef DEBUG
#define LOG_DEBUG(tag, fmt, ...)                                               \
  do {                                                                         \
    if (g_log_level <= LOG_LEVEL_DEBUG) {                                      \
      fprintf(stdout, COLOR_DEBUG "[DEBUG] " COLOR_RESET "%s - %s:%d\n" fmt "\n", \
              tag, __FILENAME__, __LINE__, ##__VA_ARGS__);                     \
    }                                                                          \
  } while (0)
#else
#define LOG_DEBUG(tag, fmt, ...) ((void)0)
#endif

#define LOG_INFO(tag, fmt, ...)                                                \
  do {                                                                         \
    if (g_log_level <= LOG_LEVEL_INFO) {                                       \
      fprintf(stdout, COLOR_INFO "[INFO]  " COLOR_RESET "%s: " fmt "\n", tag,  \
              ##__VA_ARGS__);                                                  \
    }                                                                          \
  } while (0)

#define LOG_WARN(tag, fmt, ...)                                                \
  do {                                                                         \
    if (g_log_level <= LOG_LEVEL_WARN) {                                       \
      fprintf(stderr, COLOR_WARN "[WARN]  " COLOR_RESET "%s: " fmt "\n", tag,  \
              ##__VA_ARGS__);                                                  \
    }                                                                          \
  } while (0)

#define LOG_ERR(tag, fmt, ...)                                                 \
  do {                                                                         \
    if (g_log_level <= LOG_LEVEL_ERR) {                                        \
      fprintf(stderr, COLOR_ERR "[ERR]   " COLOR_RESET "%s: " fmt "\n", tag,   \
              ##__VA_ARGS__);                                                  \
    }                                                                          \
  } while (0)

void set_log_level(log_level_t level);
log_level_t get_log_level(void);

#endif
