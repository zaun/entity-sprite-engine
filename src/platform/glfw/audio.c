#include <AL/al.h>
#include <AL/alc.h>
#include <stdbool.h>
#include "platform/audio.h"

static bool audio_initialized = false;
static ALCdevice* audio_device = NULL;
static ALCcontext* audio_context = NULL;

bool audio_startup(void) {
    if (audio_initialized) {
        return true;
    }
    
    // Open the default audio device
    audio_device = alcOpenDevice(NULL);
    if (!audio_device) {
        return false;
    }
    
    // Create an OpenAL context
    audio_context = alcCreateContext(audio_device, NULL);
    if (!audio_context) {
        alcCloseDevice(audio_device);
        audio_device = NULL;
        return false;
    }
    
    // Make the context current
    if (!alcMakeContextCurrent(audio_context)) {
        alcDestroyContext(audio_context);
        alcCloseDevice(audio_device);
        audio_context = NULL;
        audio_device = NULL;
        return false;
    }
    
    audio_initialized = true;
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
}

bool audio_is_ready(void) {
    return audio_initialized;
}
