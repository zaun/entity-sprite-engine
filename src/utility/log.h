#ifndef ESE_LOG_H
#define ESE_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

// C API functions
#ifdef __cplusplus
extern "C" {
#endif

void log_init(void);
void log_verbose(const char *category, const char *fmt, ...);
void log_debug(const char *category, const char *fmt, ...);
void log_warn(const char *category, const char *fmt, ...);
void log_error(const char *category, const char *fmt, ...);
void log_assert(const char *category, bool test, const char *fmt, ...);

// C API functions
#ifdef __cplusplus
}
#endif

#endif // ESE_LOG_H
