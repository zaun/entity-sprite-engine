#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include "platform/audio.h"

static bool audio_initialized = false;

bool audio_startup(void) {
    if (audio_initialized) {
        return true;
    }
    
    // Initialize Core Audio
    OSStatus status = AudioSessionInitialize(NULL, NULL, NULL, NULL);
    if (status != noErr) {
        return false;
    }
    
    // Set audio session category for playback
    UInt32 category = kAudioSessionCategory_MediaPlayback;
    status = AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, 
                                   sizeof(category), &category);
    if (status != noErr) {
        AudioSessionFinalize();
        return false;
    }
    
    // Activate the audio session
    status = AudioSessionSetActive(true);
    if (status != noErr) {
        AudioSessionFinalize();
        return false;
    }
    
    audio_initialized = true;
    return true;
}

void audio_shutdown(void) {
    if (!audio_initialized) {
        return;
    }
    
    // Deactivate and finalize the audio session
    AudioSessionSetActive(false);
    AudioSessionFinalize();
    
    audio_initialized = false;
}

bool audio_is_ready(void) {
    return audio_initialized;
}
