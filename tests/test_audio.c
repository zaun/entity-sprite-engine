#include "test_utils.h"
#include "platform/audio.h"
#include "core/memory_manager.h"
#include "utility/log.h"
#include <math.h>
#include <execinfo.h>
#include <signal.h>

// Test data - 1 second of 44.1kHz stereo 16-bit silence
static uint8_t test_audio_data[44100 * 2 * 2] = {0};

// Test function declarations
static void test_audio_startup_shutdown();
static void test_audio_volume_control();
static void test_audio_receiver_position();
static void test_audio_sound_creation();
static void test_audio_sound_properties();
static void test_audio_sound_playback();
static void test_audio_null_safety();

// Signal handler for testing aborts
static void segfault_handler(int sig) {
    void *array[10];
    size_t size = backtrace(array, 10);
    fprintf(stderr, "---- BACKTRACE START ----\n");
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    fprintf(stderr, "---- BACKTRACE  END  ----\n");
    exit(1);
}

int main() {
    // Set up signal handler for testing aborts
    signal(SIGSEGV, segfault_handler);
    signal(SIGABRT, segfault_handler);
    
    test_suite_begin("🔊 EseAudio Test Suite");
    
    test_audio_startup_shutdown();
    test_audio_volume_control();
    test_audio_receiver_position();
    test_audio_sound_creation();
    test_audio_sound_properties();
    test_audio_sound_playback();
    test_audio_null_safety();
    
    test_suite_end("🎯 EseAudio Test Suite");
    
    return 0;
}

static void test_audio_startup_shutdown() {
    test_begin("Audio Startup/Shutdown Tests");
    
    // Test initial state
    TEST_ASSERT(!audio_is_ready(), "Audio should not be ready initially");
    
    // Test startup
    TEST_ASSERT(audio_startup(), "Audio startup should succeed");
    TEST_ASSERT(audio_is_ready(), "Audio should be ready after startup");
    
    // Test shutdown
    audio_shutdown();
    TEST_ASSERT(!audio_is_ready(), "Audio should not be ready after shutdown");
    
    test_end("Audio Startup/Shutdown Tests");
}

static void test_audio_volume_control() {
    test_begin("Audio Volume Control Tests");
    
    TEST_ASSERT(audio_startup(), "Audio startup should succeed");
    
    // Test volume setting and getting
    audio_set_volume(0.5f);
    TEST_ASSERT_FLOAT_EQUAL(audio_get_volume(), 0.5f, 0.001f, "Volume should be 0.5");
    
    // Test volume clamping
    audio_set_volume(-0.1f);
    TEST_ASSERT_FLOAT_EQUAL(audio_get_volume(), 0.0f, 0.001f, "Volume should be clamped to 0.0");
    
    audio_set_volume(1.5f);
    TEST_ASSERT_FLOAT_EQUAL(audio_get_volume(), 1.0f, 0.001f, "Volume should be clamped to 1.0");
    
    audio_shutdown();
    test_end("Audio Volume Control Tests");
}

static void test_audio_receiver_position() {
    test_begin("Audio Receiver Position Tests");
    
    TEST_ASSERT(audio_startup(), "Audio startup should succeed");
    
    // Test setting and getting receiver position
    audio_set_receiver(10.0f, 20.0f);
    
    float x, y;
    audio_get_receiver(&x, &y);
    TEST_ASSERT_FLOAT_EQUAL(x, 10.0f, 0.001f, "Receiver X should be 10.0");
    TEST_ASSERT_FLOAT_EQUAL(y, 20.0f, 0.001f, "Receiver Y should be 20.0");
    
    // Test with NULL pointers
    audio_get_receiver(NULL, &y);
    TEST_ASSERT_FLOAT_EQUAL(y, 20.0f, 0.001f, "Receiver Y should still be 20.0");
    
    audio_get_receiver(&x, NULL);
    TEST_ASSERT_FLOAT_EQUAL(x, 10.0f, 0.001f, "Receiver X should still be 10.0");
    
    audio_shutdown();
    test_end("Audio Receiver Position Tests");
}

static void test_audio_sound_creation() {
    test_begin("Audio Sound Creation Tests");
    
    TEST_ASSERT(audio_startup(), "Audio startup should succeed");
    
    // Test sound creation with valid data
    EseSound *sound = audio_sound_create(test_audio_data);
    TEST_ASSERT_NOT_NULL(sound, "Sound creation should succeed");
    
    // Test sound properties
    TEST_ASSERT_FLOAT_EQUAL(audio_sound_get_volume(sound), 1.0f, 0.001f, "Default volume should be 1.0");
    TEST_ASSERT(!audio_sound_get_repeat(sound), "Default repeat should be false");
    TEST_ASSERT_FLOAT_EQUAL(audio_sound_get_max_distance(sound), 100.0f, 0.001f, "Default max distance should be 100.0");
    TEST_ASSERT(audio_sound_get_attenuation(sound) == AUDIO_ATTENUATION_LINEAR, "Default attenuation should be linear");
    
    // Test sound destruction
    audio_sound_destroy(sound);
    
    // Test sound creation with NULL data
    EseSound *null_sound = audio_sound_create(NULL);
    TEST_ASSERT_NULL(null_sound, "Sound creation with NULL data should fail");
    
    audio_shutdown();
    test_end("Audio Sound Creation Tests");
}

static void test_audio_sound_properties() {
    test_begin("Audio Sound Properties Tests");
    
    TEST_ASSERT(audio_startup(), "Audio startup should succeed");
    
    EseSound *sound = audio_sound_create(test_audio_data);
    TEST_ASSERT_NOT_NULL(sound, "Sound creation should succeed");
    
    // Test volume setting
    audio_sound_set_volume(sound, 0.7f);
    TEST_ASSERT_FLOAT_EQUAL(audio_sound_get_volume(sound), 0.7f, 0.001f, "Sound volume should be 0.7");
    
    // Test volume clamping
    audio_sound_set_volume(sound, -0.1f);
    TEST_ASSERT_FLOAT_EQUAL(audio_sound_get_volume(sound), 0.0f, 0.001f, "Sound volume should be clamped to 0.0");
    
    audio_sound_set_volume(sound, 1.5f);
    TEST_ASSERT_FLOAT_EQUAL(audio_sound_get_volume(sound), 1.0f, 0.001f, "Sound volume should be clamped to 1.0");
    
    // Test repeat setting
    audio_sound_set_repeat(sound, true);
    TEST_ASSERT(audio_sound_get_repeat(sound), "Sound repeat should be true");
    
    audio_sound_set_repeat(sound, false);
    TEST_ASSERT(!audio_sound_get_repeat(sound), "Sound repeat should be false");
    
    // Test position setting
    audio_sound_set_position(sound, 5.0f, 10.0f);
    
    float x, y;
    audio_sound_get_position(sound, &x, &y);
    TEST_ASSERT_FLOAT_EQUAL(x, 5.0f, 0.001f, "Sound X position should be 5.0");
    TEST_ASSERT_FLOAT_EQUAL(y, 10.0f, 0.001f, "Sound Y position should be 10.0");
    
    // Test max distance and attenuation
    audio_sound_set_max_distance(sound, 50.0f, AUDIO_ATTENUATION_EXPONENTIAL);
    TEST_ASSERT_FLOAT_EQUAL(audio_sound_get_max_distance(sound), 50.0f, 0.001f, "Max distance should be 50.0");
    TEST_ASSERT(audio_sound_get_attenuation(sound) == AUDIO_ATTENUATION_EXPONENTIAL, "Attenuation should be exponential");
    
    // Test max distance clamping
    audio_sound_set_max_distance(sound, -10.0f, AUDIO_ATTENUATION_LINEAR);
    TEST_ASSERT_FLOAT_EQUAL(audio_sound_get_max_distance(sound), 0.1f, 0.001f, "Max distance should be clamped to 0.1");
    
    audio_sound_destroy(sound);
    audio_shutdown();
    test_end("Audio Sound Properties Tests");
}

static void test_audio_sound_playback() {
    test_begin("Audio Sound Playback Tests");
    
    TEST_ASSERT(audio_startup(), "Audio startup should succeed");
    
    EseSound *sound = audio_sound_create(test_audio_data);
    TEST_ASSERT_NOT_NULL(sound, "Sound creation should succeed");
    
    // Test playback controls (these should not crash)
    audio_sound_play(sound);
    audio_sound_pause(sound);
    audio_sound_play(sound);
    audio_sound_stop(sound);
    
    // Test seeking (should not crash)
    audio_sound_seek(sound, 1000);
    
    // Test fading (should not crash)
    audio_sound_fade(sound, 0.5f, 1.0f);
    
    audio_sound_destroy(sound);
    audio_shutdown();
    test_end("Audio Sound Playback Tests");
}

static void test_audio_null_safety() {
    test_begin("Audio NULL Safety Tests");
    
    TEST_ASSERT(audio_startup(), "Audio startup should succeed");
    
    // Test all functions with NULL sound pointer
    audio_sound_destroy(NULL);
    audio_sound_set_volume(NULL, 0.5f);
    TEST_ASSERT_FLOAT_EQUAL(audio_sound_get_volume(NULL), 0.0f, 0.001f, "Volume of NULL sound should be 0.0");
    
    audio_sound_set_repeat(NULL, true);
    TEST_ASSERT(!audio_sound_get_repeat(NULL), "Repeat of NULL sound should be false");
    
    audio_sound_set_position(NULL, 1.0f, 2.0f);
    float x, y;
    audio_sound_get_position(NULL, &x, &y);
    // Should not crash
    
    audio_sound_set_max_distance(NULL, 10.0f, AUDIO_ATTENUATION_LINEAR);
    TEST_ASSERT_FLOAT_EQUAL(audio_sound_get_max_distance(NULL), 0.0f, 0.001f, "Max distance of NULL sound should be 0.0");
    TEST_ASSERT(audio_sound_get_attenuation(NULL) == AUDIO_ATTENUATION_LINEAR, "Attenuation of NULL sound should be linear");
    
    audio_sound_play(NULL);
    audio_sound_pause(NULL);
    audio_sound_stop(NULL);
    audio_sound_seek(NULL, 100);
    audio_sound_fade(NULL, 0.5f, 1.0f);
    
    TEST_ASSERT_EQUAL(0, audio_sound_get_length(NULL), "Length of NULL sound should be 0");
    TEST_ASSERT_EQUAL(0, audio_sound_get_playback_position(NULL), "Playback position of NULL sound should be 0");
    
    audio_shutdown();
    test_end("Audio NULL Safety Tests");
}