#include "logging.h"
#include <pspkernel.h>
#include <stdbool.h>

#ifdef DEBUG
log_level_t g_log_level = LOG_LEVEL_DEBUG;
#else
log_level_t g_log_level = LOG_LEVEL_INFO;
#endif

static FILE  *g_log_file = NULL;
static SceUID g_log_lock = -1;

bool log_init(const char *path) {
    g_log_lock = sceKernelCreateSema("log_lock", 0, 1, 1, NULL);
    if (g_log_lock < 0) return false;

    g_log_file = fopen(path, "w");
    if (!g_log_file) return false;

    setvbuf(g_log_file, NULL, _IOLBF, 0);
    return true;
}

void log_shutdown(void) {
    sceKernelWaitSema(g_log_lock, 1, NULL);
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    sceKernelSignalSema(g_log_lock, 1);
}

void log_write(log_level_t level, const char *level_str, const char *tag,
               const char *file, int line, const char *fmt, ...) {
    if (level < g_log_level) return;
    if (!g_log_file || g_log_lock < 0) return;

    sceKernelWaitSema(g_log_lock, 1, NULL);

    fprintf(g_log_file, "[%s] %s - %s:%d ", level_str, tag, file, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_file, fmt, args);
    va_end(args);

    fprintf(g_log_file, "\n");
    fflush(g_log_file);

    sceKernelSignalSema(g_log_lock, 1);
}

void set_log_level(log_level_t level) { g_log_level = level; }
log_level_t get_log_level(void)       { return g_log_level; }
