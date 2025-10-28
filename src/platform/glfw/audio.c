#include "platform/audio.h"
#include "core/memory_manager.h"
#include "utility/log.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <math.h>
#include <string.h>

// Internal sound structure for OpenAL
typedef struct EseOpenALSound {
  ALuint source_id;
  ALuint buffer_id;

  // Sound properties
  float volume;
  bool repeat;
  float position_x, position_y;
  float max_distance;
  EseAudioAttenuation attenuation;

  // Fade properties
  bool is_fading;
  float fade_target_volume;
  float fade_duration;
  float fade_start_time;
  float fade_start_volume;

  // Audio data
  ALsizei sample_rate;
  ALsizei channels;
  ALsizei bits_per_sample;
  size_t data_size;
  uint8_t *audio_data;
} EseOpenALSound;

// Global audio system state
static ALCdevice *audio_device = NULL;
static ALCcontext *audio_context = NULL;
static bool audio_initialized = false;
static float global_volume = 1.0f;
static float listener_x = 0.0f, listener_y = 0.0f;

// Helper function to check for OpenAL errors
static bool check_al_error(const char *operation) {
  ALenum error = alGetError();
  if (error != AL_NO_ERROR) {
    const char *error_str = "Unknown error";
    switch (error) {
    case AL_INVALID_NAME:
      error_str = "AL_INVALID_NAME";
      break;
    case AL_INVALID_ENUM:
      error_str = "AL_INVALID_ENUM";
      break;
    case AL_INVALID_VALUE:
      error_str = "AL_INVALID_VALUE";
      break;
    case AL_INVALID_OPERATION:
      error_str = "AL_INVALID_OPERATION";
      break;
    case AL_OUT_OF_MEMORY:
      error_str = "AL_OUT_OF_MEMORY";
      break;
    }
    log_error("AUDIO", "OpenAL error in %s: %s", operation, error_str);
    return false;
  }
  return true;
}

// Helper function to check for ALC errors
static bool check_alc_error(ALCdevice *device, const char *operation) {
  ALCenum error = alcGetError(device);
  if (error != ALC_NO_ERROR) {
    const char *error_str = "Unknown error";
    switch (error) {
    case ALC_INVALID_DEVICE:
      error_str = "ALC_INVALID_DEVICE";
      break;
    case ALC_INVALID_CONTEXT:
      error_str = "ALC_INVALID_CONTEXT";
      break;
    case ALC_INVALID_ENUM:
      error_str = "ALC_INVALID_ENUM";
      break;
    case ALC_INVALID_VALUE:
      error_str = "ALC_INVALID_VALUE";
      break;
    case ALC_OUT_OF_MEMORY:
      error_str = "ALC_OUT_OF_MEMORY";
      break;
    }
    log_error("AUDIO", "ALC error in %s: %s", operation, error_str);
    return false;
  }
  return true;
}

// Helper function to calculate 3D audio volume based on distance
static float calculate_3d_volume(EseOpenALSound *sound) {
  if (!sound)
    return 1.0f;

  float dx = sound->position_x - listener_x;
  float dy = sound->position_y - listener_y;
  float distance = sqrtf(dx * dx + dy * dy);

  if (distance >= sound->max_distance) {
    return 0.0f;
  }

  float volume_factor = 1.0f - (distance / sound->max_distance);

  if (sound->attenuation == AUDIO_ATTENUATION_EXPONENTIAL) {
    volume_factor = volume_factor * volume_factor;
  }

  return volume_factor;
}

// Helper function to update sound volume with 3D calculations
static void update_sound_volume(EseOpenALSound *sound) {
  if (!sound)
    return;

  float base_volume = sound->volume * global_volume;
  float distance_volume = calculate_3d_volume(sound);
  float final_volume = base_volume * distance_volume;

  alSourcef(sound->source_id, AL_GAIN, final_volume);
  check_al_error("update_sound_volume");
}

bool audio_startup(void) {
  if (audio_initialized) {
    return true;
  }

  // Open the default audio device
  audio_device = alcOpenDevice(NULL);
  if (!audio_device) {
    log_error("AUDIO", "Failed to open OpenAL device");
    return false;
  }

  if (!check_alc_error(audio_device, "alcOpenDevice")) {
    return false;
  }

  // Create an OpenAL context
  audio_context = alcCreateContext(audio_device, NULL);
  if (!audio_context) {
    log_error("AUDIO", "Failed to create OpenAL context");
    alcCloseDevice(audio_device);
    audio_device = NULL;
    return false;
  }

  if (!check_alc_error(audio_device, "alcCreateContext")) {
    alcCloseDevice(audio_device);
    audio_device = NULL;
    return false;
  }

  // Make the context current
  if (!alcMakeContextCurrent(audio_context)) {
    log_error("AUDIO", "Failed to make OpenAL context current");
    alcDestroyContext(audio_context);
    alcCloseDevice(audio_device);
    audio_context = NULL;
    audio_device = NULL;
    return false;
  }

  if (!check_alc_error(audio_device, "alcMakeContextCurrent")) {
    alcDestroyContext(audio_context);
    alcCloseDevice(audio_device);
    audio_context = NULL;
    audio_device = NULL;
    return false;
  }

  // Set default listener position
  alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
  check_al_error("alListener3f");

  audio_initialized = true;
  log_debug("AUDIO", "OpenAL audio system initialized successfully");
  return true;
}

void audio_shutdown(void) {
  if (!audio_initialized) {
    return;
  }

  // Make no context current
  alcMakeContextCurrent(NULL);

  // Destroy the context
  if (audio_context) {
    alcDestroyContext(audio_context);
    audio_context = NULL;
  }

  // Close the device
  if (audio_device) {
    alcCloseDevice(audio_device);
    audio_device = NULL;
  }

  audio_initialized = false;
  log_debug("AUDIO", "OpenAL audio system shutdown");
}

bool audio_is_ready(void) {
  return audio_initialized && audio_device != NULL && audio_context != NULL;
}

void audio_set_volume(float vol) {
  global_volume = fmaxf(0.0f, fminf(1.0f, vol));
  log_debug("AUDIO", "Global volume set to %.2f", global_volume);
}

float audio_get_volume(void) { return global_volume; }

void audio_set_receiver(float x, float y) {
  listener_x = x;
  listener_y = y;

  // Set OpenAL listener position (z=0 for 2D audio)
  alListener3f(AL_POSITION, x, y, 0.0f);
  check_al_error("alListener3f");

  log_debug("AUDIO", "Listener position set to (%.2f, %.2f)", x, y);
}

void audio_get_receiver(float *out_x, float *out_y) {
  if (out_x)
    *out_x = listener_x;
  if (out_y)
    *out_y = listener_y;
}

EseSound *audio_sound_create(const uint8_t *raw) {
  if (!audio_initialized || !raw) {
    log_error("AUDIO",
              "Cannot create sound: audio not initialized or raw data is NULL");
    return NULL;
  }

  // Allocate sound structure
  EseOpenALSound *sound = (EseOpenALSound *)memory_manager.malloc(
      sizeof(EseOpenALSound), MMTAG_AUDIO);
  if (!sound) {
    log_error("AUDIO", "Failed to allocate memory for sound");
    return NULL;
  }

  memset(sound, 0, sizeof(EseOpenALSound));

  // Generate OpenAL source and buffer
  alGenSources(1, &sound->source_id);
  if (!check_al_error("alGenSources")) {
    memory_manager.free(sound);
    return NULL;
  }

  alGenBuffers(1, &sound->buffer_id);
  if (!check_al_error("alGenBuffers")) {
    alDeleteSources(1, &sound->source_id);
    memory_manager.free(sound);
    return NULL;
  }

  // Initialize sound properties
  sound->volume = 1.0f;
  sound->repeat = false;
  sound->position_x = 0.0f;
  sound->position_y = 0.0f;
  sound->max_distance = 100.0f;
  sound->attenuation = AUDIO_ATTENUATION_LINEAR;
  sound->is_fading = false;

  // Set default audio format (assuming 44.1kHz, 16-bit, stereo)
  sound->sample_rate = 44100;
  sound->channels = 2;
  sound->bits_per_sample = 16;

  // Note: In a real implementation, you would parse the raw audio data
  // to determine the actual format and extract the PCM data.
  // For now, we'll assume the raw data is the audio data directly.
  sound->data_size = 44100 * 2 * 2; // 1 second of 44.1kHz stereo 16-bit audio
  sound->audio_data = memory_manager.malloc(sound->data_size, MMTAG_AUDIO);
  if (!sound->audio_data) {
    log_error("AUDIO", "Failed to allocate memory for audio data");
    alDeleteBuffers(1, &sound->buffer_id);
    alDeleteSources(1, &sound->source_id);
    memory_manager.free(sound);
    return NULL;
  }

  // Copy the raw data (in a real implementation, you'd parse the format)
  memcpy(sound->audio_data, raw, sound->data_size);

  // Determine OpenAL format
  ALenum format;
  if (sound->channels == 1) {
    format = (sound->bits_per_sample == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
  } else {
    format =
        (sound->bits_per_sample == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
  }

  // Upload audio data to OpenAL buffer
  alBufferData(sound->buffer_id, format, sound->audio_data,
               (ALsizei)sound->data_size, sound->sample_rate);
  if (!check_al_error("alBufferData")) {
    memory_manager.free(sound->audio_data);
    alDeleteBuffers(1, &sound->buffer_id);
    alDeleteSources(1, &sound->source_id);
    memory_manager.free(sound);
    return NULL;
  }

  // Attach buffer to source
  alSourcei(sound->source_id, AL_BUFFER, sound->buffer_id);
  check_al_error("alSourcei");

  // Set initial source properties
  alSource3f(sound->source_id, AL_POSITION, 0.0f, 0.0f, 0.0f);
  alSourcef(sound->source_id, AL_GAIN, sound->volume * global_volume);
  alSourcei(sound->source_id, AL_LOOPING, AL_FALSE);
  check_al_error("alSourcei");

  log_debug("AUDIO", "Sound created successfully");
  return (EseSound *)sound;
}

void audio_sound_destroy(EseSound *sound) {
  if (!sound)
    return;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;

  // Stop the source
  alSourceStop(openal_sound->source_id);
  check_al_error("alSourceStop");

  // Clean up OpenAL resources
  alDeleteSources(1, &openal_sound->source_id);
  alDeleteBuffers(1, &openal_sound->buffer_id);
  check_al_error("alDeleteSources/Buffers");

  // Free audio data
  if (openal_sound->audio_data) {
    memory_manager.free(openal_sound->audio_data);
  }

  memory_manager.free(openal_sound);
  log_debug("AUDIO", "Sound destroyed");
}

void audio_sound_set_position(EseSound *sound, float x, float y) {
  if (!sound)
    return;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;
  openal_sound->position_x = x;
  openal_sound->position_y = y;

  // Set OpenAL source position (z=0 for 2D audio)
  alSource3f(openal_sound->source_id, AL_POSITION, x, y, 0.0f);
  check_al_error("alSource3f");

  update_sound_volume(openal_sound);
}

void audio_sound_get_position(EseSound *sound, float *out_x, float *out_y) {
  if (!sound)
    return;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;
  if (out_x)
    *out_x = openal_sound->position_x;
  if (out_y)
    *out_y = openal_sound->position_y;
}

void audio_sound_set_repeat(EseSound *sound, bool value) {
  if (!sound)
    return;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;
  openal_sound->repeat = value;

  alSourcei(openal_sound->source_id, AL_LOOPING, value ? AL_TRUE : AL_FALSE);
  check_al_error("alSourcei");
}

bool audio_sound_get_repeat(EseSound *sound) {
  if (!sound)
    return false;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;
  return openal_sound->repeat;
}

void audio_sound_set_volume(EseSound *sound, float value) {
  if (!sound)
    return;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;
  openal_sound->volume = fmaxf(0.0f, fminf(1.0f, value));

  update_sound_volume(openal_sound);
}

float audio_sound_get_volume(EseSound *sound) {
  if (!sound)
    return 0.0f;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;
  return openal_sound->volume;
}

size_t audio_sound_get_length(EseSound *sound) {
  if (!sound)
    return 0;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;
  return openal_sound->data_size /
         (openal_sound->channels * (openal_sound->bits_per_sample / 8));
}

size_t audio_sound_get_playback_position(EseSound *sound) {
  if (!sound)
    return 0;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;

  ALint offset;
  alGetSourcei(openal_sound->source_id, AL_SAMPLE_OFFSET, &offset);
  check_al_error("alGetSourcei");

  return (size_t)offset;
}

void audio_sound_set_max_distance(EseSound *sound, float max_dist,
                                  EseAudioAttenuation attenuation) {
  if (!sound)
    return;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;
  openal_sound->max_distance = fmaxf(0.1f, max_dist);
  openal_sound->attenuation = attenuation;

  // Set OpenAL distance model
  ALenum distance_model = (attenuation == AUDIO_ATTENUATION_LINEAR)
                              ? AL_LINEAR_DISTANCE
                              : AL_EXPONENT_DISTANCE;
  alDistanceModel(distance_model);
  check_al_error("alDistanceModel");

  // Set reference distance and max distance
  alSourcef(openal_sound->source_id, AL_REFERENCE_DISTANCE, 1.0f);
  alSourcef(openal_sound->source_id, AL_MAX_DISTANCE, max_dist);
  check_al_error("alSourcef");

  update_sound_volume(openal_sound);
}

float audio_sound_get_max_distance(EseSound *sound) {
  if (!sound)
    return 0.0f;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;
  return openal_sound->max_distance;
}

EseAudioAttenuation audio_sound_get_attenuation(EseSound *sound) {
  if (!sound)
    return AUDIO_ATTENUATION_LINEAR;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;
  return openal_sound->attenuation;
}

void audio_sound_play(EseSound *sound) {
  if (!sound)
    return;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;

  alSourcePlay(openal_sound->source_id);
  check_al_error("alSourcePlay");
}

void audio_sound_pause(EseSound *sound) {
  if (!sound)
    return;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;

  alSourcePause(openal_sound->source_id);
  check_al_error("alSourcePause");
}

void audio_sound_stop(EseSound *sound) {
  if (!sound)
    return;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;

  alSourceStop(openal_sound->source_id);
  check_al_error("alSourceStop");
}

void audio_sound_seek(EseSound *sound, size_t position) {
  if (!sound)
    return;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;

  alSourcei(openal_sound->source_id, AL_SAMPLE_OFFSET, (ALint)position);
  check_al_error("alSourcei");
}

void audio_sound_fade(EseSound *sound, float target_volume, float duration) {
  if (!sound || duration <= 0.0f)
    return;

  EseOpenALSound *openal_sound = (EseOpenALSound *)sound;
  openal_sound->is_fading = true;
  openal_sound->fade_target_volume = fmaxf(0.0f, fminf(1.0f, target_volume));
  openal_sound->fade_duration = duration;
  openal_sound->fade_start_time = 0.0f; // Would need actual time function
  openal_sound->fade_start_volume = openal_sound->volume;

  // Note: The actual fade implementation would need to be handled
  // in a separate update loop that calls update_sound_volume periodically
  log_debug("AUDIO", "Fade started: %.2f -> %.2f over %.2fs",
            openal_sound->fade_start_volume, target_volume, duration);
}