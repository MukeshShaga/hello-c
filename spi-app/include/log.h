/* log.h - tiny leveled logger */
#ifndef LOG_H
#define LOG_H

#include <stddef.h>

typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE, /* also dumps raw SPI bytes in the mock backend */
} log_level_t;

void        log_set_level(log_level_t level);
log_level_t log_get_level(void);
void        log_write(log_level_t level, const char *file, int line,
                      const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));
void        log_hexdump(const char *label, const void *data, size_t len);

#define LOG_ERR(...) log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WRN(...) log_write(LOG_LEVEL_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INF(...) log_write(LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DBG(...) log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

#endif /* LOG_H */
