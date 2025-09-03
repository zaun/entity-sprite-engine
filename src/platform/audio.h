#ifndef ESE_PLATFORM_AUDIO_H
#define ESE_PLATFORM_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration for opaque Sound type
typedef struct EseSound EseSound;

// Attenuation types for 3D audio
typedef enum {
    AUDIO_ATTENUATION_LINEAR = 0,
    AUDIO_ATTENUATION_EXPONENTIAL = 1
} EseAudioAttenuation;

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

/**
 * Set the global audio volume (0.0 to 1.0).
 * 
 * @param vol Volume level from 0.0 (silent) to 1.0 (full volume)
 */
void audio_set_volume(float vol);

/**
 * Get the current global audio volume.
 * 
 * @return Current volume level from 0.0 to 1.0
 */
float audio_get_volume(void);

/**
 * Set the position of the audio listener/receiver for 3D audio.
 * 
 * @param x X coordinate of the listener
 * @param y Y coordinate of the listener
 */
void audio_set_receiver(float x, float y);

/**
 * Get the current position of the audio listener/receiver.
 * 
 * @param out_x Pointer to store the X coordinate
 * @param out_y Pointer to store the Y coordinate
 */
void audio_get_receiver(float *out_x, float *out_y);

/**
 * Create a new sound from raw audio data.
 * 
 * @param raw Raw audio data (PCM format)
 * @return Pointer to the created sound, or NULL on failure
 */
EseSound* audio_sound_create(const uint8_t *raw);

/**
 * Destroy a sound and free its resources.
 * 
 * @param sound Pointer to the sound to destroy
 */
void audio_sound_destroy(EseSound *sound);

/**
 * Set the position of a 3D sound source.
 * 
 * @param sound Pointer to the sound
 * @param x X coordinate of the sound source
 * @param y Y coordinate of the sound source
 */
void audio_sound_set_position(EseSound *sound, float x, float y);

/**
 * Get the current position of a 3D sound source.
 * 
 * @param sound Pointer to the sound
 * @param out_x Pointer to store the X coordinate
 * @param out_y Pointer to store the Y coordinate
 */
void audio_sound_get_position(EseSound *sound, float *out_x, float *out_y);

/**
 * Set whether a sound should repeat/loop.
 * 
 * @param sound Pointer to the sound
 * @param value true to enable looping, false to disable
 */
void audio_sound_set_repeat(EseSound *sound, bool value);

/**
 * Get whether a sound is set to repeat/loop.
 * 
 * @param sound Pointer to the sound
 * @return true if looping is enabled, false otherwise
 */
bool audio_sound_get_repeat(EseSound *sound);

/**
 * Set the volume of a specific sound (0.0 to 1.0).
 * 
 * @param sound Pointer to the sound
 * @param value Volume level from 0.0 to 1.0
 */
void audio_sound_set_volume(EseSound *sound, float value);

/**
 * Get the current volume of a specific sound.
 * 
 * @param sound Pointer to the sound
 * @return Current volume level from 0.0 to 1.0
 */
float audio_sound_get_volume(EseSound *sound);

/**
 * Get the total length of a sound in samples.
 * 
 * @param sound Pointer to the sound
 * @return Length in samples
 */
size_t audio_sound_get_length(EseSound *sound);

/**
 * Get the current playback position of a sound in samples.
 * 
 * @param sound Pointer to the sound
 * @return Current position in samples
 */
size_t audio_sound_get_playback_position(EseSound *sound);

/**
 * Set the maximum distance for 3D audio attenuation.
 * 
 * @param sound Pointer to the sound
 * @param max_dist Maximum distance for full volume
 * @param attenuation Type of distance attenuation to use
 */
void audio_sound_set_max_distance(EseSound *sound, float max_dist, EseAudioAttenuation attenuation);

/**
 * Get the maximum distance for 3D audio attenuation.
 * 
 * @param sound Pointer to the sound
 * @return Maximum distance setting
 */
float audio_sound_get_max_distance(EseSound *sound);

/**
 * Get the current attenuation type for a sound.
 * 
 * @param sound Pointer to the sound
 * @return Current attenuation type
 */
EseAudioAttenuation audio_sound_get_attenuation(EseSound *sound);

/**
 * Start playing a sound.
 * 
 * @param sound Pointer to the sound to play
 */
void audio_sound_play(EseSound *sound);

/**
 * Pause a currently playing sound.
 * 
 * @param sound Pointer to the sound to pause
 */
void audio_sound_pause(EseSound *sound);

/**
 * Stop a sound and reset its position to the beginning.
 * 
 * @param sound Pointer to the sound to stop
 */
void audio_sound_stop(EseSound *sound);

/**
 * Seek to a specific position in a sound.
 * 
 * @param sound Pointer to the sound
 * @param position Position in samples to seek to
 */
void audio_sound_seek(EseSound *sound, size_t position);

/**
 * Fade a sound to a target volume over a specified duration.
 * 
 * @param sound Pointer to the sound
 * @param target_volume Target volume to fade to (0.0 to 1.0)
 * @param duration Duration of the fade in seconds
 */
void audio_sound_fade(EseSound *sound, float target_volume, float duration);

#ifdef __cplusplus
}
#endif

#endif // ESE_PLATFORM_AUDIO_H