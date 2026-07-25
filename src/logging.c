#include "logging.h"

#ifdef DEBUG
log_level_t g_log_level = LOG_LEVEL_DEBUG;
#else
log_level_t g_log_level = LOG_LEVEL_INFO;
#endif

void set_log_level(log_level_t level) {
  g_log_level = level;
}

log_level_t get_log_level(void) {
  return g_log_level;
}
