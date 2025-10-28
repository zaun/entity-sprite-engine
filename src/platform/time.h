#ifndef ESE_PLATFORM_TIME_H
#define ESE_PLATFORM_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the current time in nanoseconds since an arbitrary reference point.
 * This is monotonic time, suitable for measuring intervals.
 *
 * @return Current time in nanoseconds
 */
uint64_t time_now(void);

/**
 * Get the current time in seconds since an arbitrary reference point.
 * This is monotonic time, suitable for measuring intervals.
 *
 * @return Current time in seconds as a double
 */
double time_now_seconds(void);

/**
 * Get the time conversion factor for converting raw time values to seconds.
 * This is useful for platforms that need custom time conversion.
 *
 * @param numer Pointer to store the numerator
 * @param denom Pointer to store the denominator
 */
void time_get_conversion_factor(uint32_t *numer, uint32_t *denom);

#ifdef __cplusplus
}
#endif

#endif // ESE_PLATFORM_TIME_H
