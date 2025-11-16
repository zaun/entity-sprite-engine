#include "utility/log.h"
#include <execinfo.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "utility/thread.h"

static struct timeval log_start_time;
static char *log_enabled_categories = NULL;
static bool log_verbose_en = false;

void log_init(void) {
    gettimeofday(&log_start_time, NULL);
    const char *env = getenv("LOG_CATEGORIES");
    if (env) {
        if (log_enabled_categories) {
            free(log_enabled_categories);
        }
        log_enabled_categories = strdup(env);
    }
    const char *env_verbose = getenv("LOG_VERBOSE");
    if (env_verbose) {
        log_verbose_en = true;
    }
}

static int log_category_enabled(const char *category) {
    if (!log_enabled_categories) {
        return 0;
    }

    // Check if "ALL" is specified to enable all categories
    if (strcmp(log_enabled_categories, "ALL") == 0) {
        return 1;
    }

    // Tokenize the enabled categories (comma-separated)
    char *categories = strdup(log_enabled_categories);
    char *token = strtok(categories, ",");
    while (token) {
        if (strcmp(token, category) == 0) {
            free(categories);
            return 1;
        }
        token = strtok(NULL, ",");
    }
    free(categories);
    return 0;
}

static void log_vprint(const char *level, const char *category, const char *fmt, va_list args) {
    if (strcmp(level, "AST") != 0 && strcmp(level, "ERR") != 0 && !log_category_enabled(category)) {
        return;
    }

    if (strcmp(level, "VRB") == 0 && !log_verbose_en) {
        return;
    }

    int32_t thread_id = ese_thread_get_number();

    struct timeval now;
    gettimeofday(&now, NULL);

    long seconds = now.tv_sec - log_start_time.tv_sec;
    long usec = now.tv_usec - log_start_time.tv_usec;
    if (usec < 0) {
        seconds -= 1;
        usec += 1000000;
    }
    long ms = usec / 1000;

    printf("[%04ld:%03ld] [%02d:%s] [%s] ", seconds, ms, thread_id, level, category);
    vprintf(fmt, args);
    printf("\n");
}

void log_verbose(const char *category, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_vprint("VRB", category, fmt, args);
    va_end(args);
}

void log_debug(const char *category, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_vprint("DBG", category, fmt, args);
    va_end(args);
}

void log_warn(const char *category, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_vprint("WRN", category, fmt, args);
    va_end(args);
}

void log_error(const char *category, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_vprint("ERR", category, fmt, args);
    va_end(args);
}

void log_assert(const char *category, bool test, const char *fmt, ...) {
    if (!test) {
        va_list args;
        va_start(args, fmt);
        log_vprint("AST", category, fmt, args);
        va_end(args);

        void *buffer[32];
        int nptrs = backtrace(buffer, 32);
        char **strings = backtrace_symbols(buffer, nptrs);
        if (strings) {
            fprintf(stderr, "---- BACKTRACE START ----\n");
            for (int i = 0; i < nptrs; i++) {
                fprintf(stderr, "%s\n", strings[i]);
            }
            fprintf(stderr, "---- BACKTRACE  END  ----\n");
            free(strings);
        }

        abort();
    }
}
