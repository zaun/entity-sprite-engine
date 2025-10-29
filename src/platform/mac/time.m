#include "platform/time.h"
#include <mach/mach_time.h>

uint64_t time_now(void) { return mach_absolute_time(); }

double time_now_seconds(void) {
    static mach_timebase_info_data_t timebase = {0};
    static int initialized = 0;

    if (!initialized) {
        mach_timebase_info(&timebase);
        initialized = 1;
    }

    uint64_t time = mach_absolute_time();
    return (double)time * (double)timebase.numer / (double)timebase.denom / 1e9;
}

void time_get_conversion_factor(uint32_t *numer, uint32_t *denom) {
    static mach_timebase_info_data_t timebase = {0};
    static int initialized = 0;

    if (!initialized) {
        mach_timebase_info(&timebase);
        initialized = 1;
    }

    *numer = timebase.numer;
    *denom = timebase.denom;
}
