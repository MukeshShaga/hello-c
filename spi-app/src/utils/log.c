/* log.c - tiny leveled logger writing to stderr */
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static log_level_t current_level = LOG_LEVEL_INFO;

static const char *const level_names[] = { "ERR", "WRN", "INF", "DBG", "TRC" };

void log_set_level(log_level_t level) { current_level = level; }
log_level_t log_get_level(void) { return current_level; }

void log_write(log_level_t level, const char *file, int line,
               const char *fmt, ...)
{
    if (level > current_level)
        return;

    const char *base = strrchr(file, '/');
    base = base ? base + 1 : file;

    fprintf(stderr, "[%s] %s:%d: ", level_names[level], base, line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

void log_hexdump(const char *label, const void *data, size_t len)
{
    const unsigned char *p = data;

    fprintf(stderr, "%s (%zu):", label, len);
    for (size_t i = 0; i < len; i++) {
        if (i && i % 16 == 0)
            fprintf(stderr, "\n%*s", (int)strlen(label) + 1, "");
        fprintf(stderr, " %02X", p[i]);
    }
    fputc('\n', stderr);
}
