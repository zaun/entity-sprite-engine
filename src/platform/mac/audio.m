#import <AVFoundation/AVFoundation.h>
#import <CoreAudio/CoreAudio.h>
#import "platform/audio.h"
#import "core/memory_manager.h"
#import "utility/log.h"

// Internal sound structure for macOS
typedef struct EseMacSound {
    AVAudioPlayerNode *playerNode;
    AVAudioPCMBuffer *audioBuffer;
    AVAudioFormat *format;
    
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
} EseMacSound;

// Global audio system state
static AVAudioEngine *audio_engine = nil;
static AVAudioPlayerNode *master_player = nil;
static AVAudioMixerNode *master_mixer = nil;
static bool audio_initialized = false;
static float global_volume = 1.0f;
static float listener_x = 0.0f, listener_y = 0.0f;

// Helper function to calculate 3D audio volume based on distance
static float calculate_3d_volume(EseMacSound *sound) {
    if (!sound) return 1.0f;
    
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
static void update_sound_volume(EseMacSound *sound) {
    if (!sound || !sound->playerNode) return;
    
    float base_volume = sound->volume * global_volume;
    float distance_volume = calculate_3d_volume(sound);
    float final_volume = base_volume * distance_volume;
    
    [sound->playerNode setVolume:final_volume];
}

bool audio_startup(void) {
    if (audio_initialized) {
        return true;
    }
    
    @try {
        // Create audio engine
        audio_engine = [[AVAudioEngine alloc] init];
        if (!audio_engine) {
            log_error("AUDIO", "Failed to create AVAudioEngine");
            return false;
        }
        
        // Get the main mixer node
        master_mixer = [audio_engine mainMixerNode];
        if (!master_mixer) {
            log_error("AUDIO", "Failed to get main mixer node");
            return false;
        }
        
        // Start the audio engine
        NSError *error = nil;
        if (![audio_engine startAndReturnError:&error]) {
            log_error("AUDIO", "Failed to start audio engine: %s", 
                     error.localizedDescription.UTF8String);
            return false;
        }
        
        audio_initialized = true;
        log_debug("AUDIO", "macOS audio system initialized successfully");
        return true;
        
    } @catch (NSException *exception) {
        log_error("AUDIO", "Exception during audio startup: %s", 
                 exception.reason.UTF8String);
        return false;
    }
}

void audio_shutdown(void) {
    if (!audio_initialized) {
        return;
    }
    
    @try {
        if (audio_engine) {
            [audio_engine stop];
            audio_engine = nil;
        }
        
        master_mixer = nil;
        audio_initialized = false;
        log_debug("AUDIO", "macOS audio system shutdown");
        
    } @catch (NSException *exception) {
        log_error("AUDIO", "Exception during audio shutdown: %s", 
                 exception.reason.UTF8String);
    }
}

bool audio_is_ready(void) {
    return audio_initialized && audio_engine != nil;
}

void audio_set_volume(float vol) {
    global_volume = fmaxf(0.0f, fminf(1.0f, vol));
    log_debug("AUDIO", "Global volume set to %.2f", global_volume);
}

float audio_get_volume(void) {
    return global_volume;
}

void audio_set_receiver(float x, float y) {
    listener_x = x;
    listener_y = y;
    log_debug("AUDIO", "Listener position set to (%.2f, %.2f)", x, y);
}

void audio_get_receiver(float *out_x, float *out_y) {
    if (out_x) *out_x = listener_x;
    if (out_y) *out_y = listener_y;
}

EseSound* audio_sound_create(const uint8_t *raw) {
    if (!audio_initialized || !raw) {
        log_error("AUDIO", "Cannot create sound: audio not initialized or raw data is NULL");
        return NULL;
    }
    
    @try {
        // Allocate sound structure
        EseMacSound *sound = (EseMacSound *)memory_manager.malloc(sizeof(EseMacSound), MMTAG_AUDIO);
        if (!sound) {
            log_error("AUDIO", "Failed to allocate memory for sound");
            return NULL;
        }
        
        memset(sound, 0, sizeof(EseMacSound));
        
        // Create audio format (assuming 44.1kHz, 16-bit, stereo)
        sound->format = [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                         sampleRate:44100.0
                                                           channels:2
                                                        interleaved:NO];
        if (!sound->format) {
            log_error("AUDIO", "Failed to create audio format");
            memory_manager.free(sound);
            return NULL;
        }
        
        // Create player node
        sound->playerNode = [[AVAudioPlayerNode alloc] init];
        if (!sound->playerNode) {
            log_error("AUDIO", "Failed to create player node");
            memory_manager.free(sound);
            return NULL;
        }
        
        // Attach player node to engine
        [audio_engine attachNode:sound->playerNode];
        
        // Connect to main mixer
        [audio_engine connect:sound->playerNode to:master_mixer format:sound->format];
        
        // Initialize sound properties
        sound->volume = 1.0f;
        sound->repeat = false;
        sound->position_x = 0.0f;
        sound->position_y = 0.0f;
        sound->max_distance = 100.0f;
        sound->attenuation = AUDIO_ATTENUATION_LINEAR;
        sound->is_fading = false;
        
        // Note: In a real implementation, you would parse the raw audio data
        // and create an AVAudioPCMBuffer from it. For now, we'll create an empty buffer.
        AVAudioFrameCount frameCount = 44100; // 1 second of audio
        sound->audioBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:sound->format 
                                                           frameCapacity:frameCount];
        
        if (!sound->audioBuffer) {
            log_error("AUDIO", "Failed to create audio buffer");
            [audio_engine detachNode:sound->playerNode];
            memory_manager.free(sound);
            return NULL;
        }
        
        log_debug("AUDIO", "Sound created successfully");
        return (EseSound *)sound;
        
    } @catch (NSException *exception) {
        log_error("AUDIO", "Exception creating sound: %s", 
                 exception.reason.UTF8String);
        return NULL;
    }
}

void audio_sound_destroy(EseSound *sound) {
    if (!sound) return;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    
    @try {
        if (mac_sound->playerNode) {
            [mac_sound->playerNode stop];
            [audio_engine detachNode:mac_sound->playerNode];
        }
        
        memory_manager.free(mac_sound);
        log_debug("AUDIO", "Sound destroyed");
        
    } @catch (NSException *exception) {
        log_error("AUDIO", "Exception destroying sound: %s", 
                 exception.reason.UTF8String);
    }
}

void audio_sound_set_position(EseSound *sound, float x, float y) {
    if (!sound) return;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    mac_sound->position_x = x;
    mac_sound->position_y = y;
    
    update_sound_volume(mac_sound);
}

void audio_sound_get_position(EseSound *sound, float *out_x, float *out_y) {
    if (!sound) return;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    if (out_x) *out_x = mac_sound->position_x;
    if (out_y) *out_y = mac_sound->position_y;
}

void audio_sound_set_repeat(EseSound *sound, bool value) {
    if (!sound) return;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    mac_sound->repeat = value;
    
    // Note: AVAudioPlayerNode doesn't have built-in looping, so this would need
    // to be implemented with a completion handler that restarts playback
}

bool audio_sound_get_repeat(EseSound *sound) {
    if (!sound) return false;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    return mac_sound->repeat;
}

void audio_sound_set_volume(EseSound *sound, float value) {
    if (!sound) return;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    mac_sound->volume = fmaxf(0.0f, fminf(1.0f, value));
    
    update_sound_volume(mac_sound);
}

float audio_sound_get_volume(EseSound *sound) {
    if (!sound) return 0.0f;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    return mac_sound->volume;
}

size_t audio_sound_get_length(EseSound *sound) {
    if (!sound) return 0;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    if (!mac_sound->audioBuffer) return 0;
    
    return (size_t)mac_sound->audioBuffer.frameLength;
}

size_t audio_sound_get_playback_position(EseSound *sound) {
    if (!sound) return 0;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    if (!mac_sound->playerNode) return 0;
    
    // Note: AVAudioPlayerNode doesn't provide direct access to current position
    // This would need to be tracked manually or use a different approach
    return 0;
}

void audio_sound_set_max_distance(EseSound *sound, float max_dist, EseAudioAttenuation attenuation) {
    if (!sound) return;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    mac_sound->max_distance = fmaxf(0.1f, max_dist);
    mac_sound->attenuation = attenuation;
    
    update_sound_volume(mac_sound);
}

float audio_sound_get_max_distance(EseSound *sound) {
    if (!sound) return 0.0f;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    return mac_sound->max_distance;
}

EseAudioAttenuation audio_sound_get_attenuation(EseSound *sound) {
    if (!sound) return AUDIO_ATTENUATION_LINEAR;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    return mac_sound->attenuation;
}

void audio_sound_play(EseSound *sound) {
    if (!sound) return;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    
    @try {
        if (mac_sound->playerNode && mac_sound->audioBuffer) {
            [mac_sound->playerNode scheduleBuffer:mac_sound->audioBuffer 
                                          atTime:nil 
                                         options:0 
                                 completionHandler:nil];
            [mac_sound->playerNode play];
        }
    } @catch (NSException *exception) {
        log_error("AUDIO", "Exception playing sound: %s", 
                 exception.reason.UTF8String);
    }
}

void audio_sound_pause(EseSound *sound) {
    if (!sound) return;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    
    @try {
        if (mac_sound->playerNode) {
            [mac_sound->playerNode pause];
        }
    } @catch (NSException *exception) {
        log_error("AUDIO", "Exception pausing sound: %s", 
                 exception.reason.UTF8String);
    }
}

void audio_sound_stop(EseSound *sound) {
    if (!sound) return;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    
    @try {
        if (mac_sound->playerNode) {
            [mac_sound->playerNode stop];
        }
    } @catch (NSException *exception) {
        log_error("AUDIO", "Exception stopping sound: %s", 
                 exception.reason.UTF8String);
    }
}

void audio_sound_seek(EseSound *sound, size_t position) {
    if (!sound) return;
    
    // Note: AVAudioPlayerNode doesn't support seeking directly
    // This would need to be implemented by stopping and restarting playback
    // at the desired position, or by using a different audio framework
    log_debug("AUDIO", "Seek not implemented for macOS audio");
}

void audio_sound_fade(EseSound *sound, float target_volume, float duration) {
    if (!sound || duration <= 0.0f) return;
    
    EseMacSound *mac_sound = (EseMacSound *)sound;
    mac_sound->is_fading = true;
    mac_sound->fade_target_volume = fmaxf(0.0f, fminf(1.0f, target_volume));
    mac_sound->fade_duration = duration;
    mac_sound->fade_start_time = CACurrentMediaTime();
    mac_sound->fade_start_volume = mac_sound->volume;
    
    // Note: The actual fade implementation would need to be handled
    // in a separate update loop that calls update_sound_volume periodically
    log_debug("AUDIO", "Fade started: %.2f -> %.2f over %.2fs", 
             mac_sound->fade_start_volume, target_volume, duration);
}