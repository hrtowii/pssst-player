#ifndef LOGGING_H
#define LOGGING_H
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#define LOG_TO_SCREEN


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
#if defined(LOG_TO_FILE)

extern FILE *g_log_file;

// Call once at startup, e.g. log_file_open("ms0:/gator_log.txt")
// (use "host0:/gator_log.txt" when running under PPSSPP with host filesystem
// mounted, so you can open it directly from your PC).
static inline int log_file_open(const char *path) {
  g_log_file = fopen(path, "w");
  if (g_log_file) setvbuf(g_log_file, NULL, _IOLBF, 0); // line-buffered so a
                                                          // crash doesn't eat
                                                          // your last lines
  return g_log_file != NULL;
}

static inline void log_file_close(void) {
  if (g_log_file) {
    fclose(g_log_file);
    g_log_file = NULL;
  }
}

#define LOG_SINK (g_log_file ? g_log_file : stdout)
#define LOG_SINK_ERR (g_log_file ? g_log_file : stderr)
// No ANSI color codes when writing to a file, they just show up as garbage.
#define LOG_COLOR_DEBUG ""
#define LOG_COLOR_INFO ""
#define LOG_COLOR_WARN ""
#define LOG_COLOR_ERR ""
#define LOG_COLOR_RESET ""

#elif defined(LOG_TO_SCREEN)

#include <pspdebug.h>

// pspDebugScreenPrintf has no fprintf-style stream arg and no color codes
// worth using (psp debug screen ignores them), so route everything through
// one helper.
static inline void log_screen_printf(const char *fmt, ...) {
  char buf[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  pspDebugScreenPrintf("%s", buf);
}

#else // default: original console behavior

#define LOG_SINK stdout
#define LOG_SINK_ERR stderr
#define LOG_COLOR_DEBUG COLOR_DEBUG
#define LOG_COLOR_INFO COLOR_INFO
#define LOG_COLOR_WARN COLOR_WARN
#define LOG_COLOR_ERR COLOR_ERR
#define LOG_COLOR_RESET COLOR_RESET

#endif

// ---------------------------------------------------------------------------
// Macros
// ---------------------------------------------------------------------------

#if defined(LOG_TO_SCREEN)

// DEBUG logging is only compiled for debug builds
#ifdef DEBUG
#define LOG_DEBUG(tag, fmt, ...)                                               \
  do {                                                                         \
    if (g_log_level <= LOG_LEVEL_DEBUG) {                                      \
      log_screen_printf("[DEBUG] %s - %s:%d\n" fmt "\n", tag, __FILENAME__,    \
                         __LINE__, ##__VA_ARGS__);                             \
    }                                                                          \
  } while (0)
#else
#define LOG_DEBUG(tag, fmt, ...) ((void)0)
#endif

#define LOG_INFO(tag, fmt, ...)                                                \
  do {                                                                         \
    if (g_log_level <= LOG_LEVEL_INFO) {                                       \
      log_screen_printf("[INFO]  %s: " fmt "\n", tag, ##__VA_ARGS__);          \
    }                                                                          \
  } while (0)
#define LOG_WARN(tag, fmt, ...)                                                \
  do {                                                                         \
    if (g_log_level <= LOG_LEVEL_WARN) {                                       \
      log_screen_printf("[WARN]  %s: " fmt "\n", tag, ##__VA_ARGS__);          \
    }                                                                          \
  } while (0)
#define LOG_ERR(tag, fmt, ...)                                                 \
  do {                                                                         \
    if (g_log_level <= LOG_LEVEL_ERR) {                                        \
      log_screen_printf("[ERR]   %s: " fmt "\n", tag, ##__VA_ARGS__);          \
    }                                                                          \
  } while (0)

#else

#ifdef DEBUG
#define LOG_DEBUG(tag, fmt, ...)                                               \
  do {                                                                         \
    if (g_log_level <= LOG_LEVEL_DEBUG) {                                      \
      fprintf(LOG_SINK,                                                        \
              LOG_COLOR_DEBUG "[DEBUG] " LOG_COLOR_RESET "%s - %s:%d\n" fmt     \
              "\n",                                                            \
              tag, __FILENAME__, __LINE__, ##__VA_ARGS__);                     \
    }                                                                          \
  } while (0)
#else
#define LOG_DEBUG(tag, fmt, ...) ((void)0)
#endif

#define LOG_INFO(tag, fmt, ...)                                                \
  do {                                                                         \
    if (g_log_level <= LOG_LEVEL_INFO) {                                       \
      fprintf(LOG_SINK, LOG_COLOR_INFO "[INFO]  " LOG_COLOR_RESET "%s: " fmt   \
              "\n", tag, ##__VA_ARGS__);                                       \
    }                                                                          \
  } while (0)
#define LOG_WARN(tag, fmt, ...)                                                \
  do {                                                                         \
    if (g_log_level <= LOG_LEVEL_WARN) {                                       \
      fprintf(LOG_SINK_ERR, LOG_COLOR_WARN "[WARN]  " LOG_COLOR_RESET "%s: "   \
              fmt "\n", tag, ##__VA_ARGS__);                                   \
    }                                                                          \
  } while (0)
#define LOG_ERR(tag, fmt, ...)                                                 \
  do {                                                                         \
    if (g_log_level <= LOG_LEVEL_ERR) {                                        \
      fprintf(LOG_SINK_ERR, LOG_COLOR_ERR "[ERR]   " LOG_COLOR_RESET "%s: "    \
              fmt "\n", tag, ##__VA_ARGS__);                                   \
    }                                                                          \
  } while (0)

#endif

void set_log_level(log_level_t level);
log_level_t get_log_level(void);

#endif
