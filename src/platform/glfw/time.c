#include "platform/time.h"
#include <time.h>

uint64_t time_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

double time_now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

void time_get_conversion_factor(uint32_t *numer, uint32_t *denom) {
    // For POSIX systems, we use direct conversion (1:1 ratio)
    *numer = 1;
    *denom = 1;
}
