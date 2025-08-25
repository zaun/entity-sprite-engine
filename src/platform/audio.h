#ifndef ESE_PLATFORM_AUDIO_H
#define ESE_PLATFORM_AUDIO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the audio system for the current platform.
 * This should be called before any audio operations.
 * 
 * @return true if initialization was successful, false otherwise
 */
bool audio_startup(void);

/**
 * Shutdown the audio system and clean up resources.
 * This should be called when the application is terminating.
 */
void audio_shutdown(void);

/**
 * Check if the audio system is currently initialized and ready.
 * 
 * @return true if audio system is ready, false otherwise
 */
bool audio_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif // ESE_PLATFORM_AUDIO_H
