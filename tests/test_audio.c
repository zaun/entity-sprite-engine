/*
* test_ese_audio.c - Unity-based tests for audio functionality
*/

#include "testing.h"

#include "../src/platform/audio.h"
#include "../src/core/memory_manager.h"
#include "../src/utility/log.h"
#include <math.h>

/**
* Test data - 1 second of 44.1kHz stereo 16-bit silence
*/
static uint8_t test_audio_data[44100 * 2 * 2] = {0};

/**
* Test Functions Declarations
*/
static void test_ese_audio_startup_shutdown(void);
static void test_ese_audio_volume_control(void);
static void test_ese_audio_receiver_position(void);
static void test_ese_audio_sound_creation(void);
static void test_ese_audio_sound_properties(void);
static void test_ese_audio_sound_playback(void);
static void test_ese_audio_null_safety(void);

/**
* Test suite setup and teardown
*/
void setUp(void) {
    /* Ensure clean state per test if needed */
}

void tearDown(void) {
    /* Ensure audio is shut down after each test */
    audio_shutdown();
}

/**
* Main test runner
*/
int main(void) {
    log_init();

    printf("\nEseAudio Tests\n");
    printf("--------------\n");

    UNITY_BEGIN();

    RUN_TEST(test_ese_audio_startup_shutdown);
    RUN_TEST(test_ese_audio_volume_control);
    RUN_TEST(test_ese_audio_receiver_position);
    RUN_TEST(test_ese_audio_sound_creation);
    RUN_TEST(test_ese_audio_sound_properties);
    RUN_TEST(test_ese_audio_sound_playback);
    RUN_TEST(test_ese_audio_null_safety);

    memory_manager.destroy(true);

    return UNITY_END();
}

/**
* Test Implementations
*/

static void test_ese_audio_startup_shutdown(void) {
    /* Initial state */
    TEST_ASSERT_FALSE_MESSAGE(audio_is_ready(), "Audio should not be ready initially");

    /* Startup */
    TEST_ASSERT_TRUE_MESSAGE(audio_startup(), "Audio startup should succeed");
    TEST_ASSERT_TRUE_MESSAGE(audio_is_ready(), "Audio should be ready after startup");

    /* Shutdown */
    audio_shutdown();
    TEST_ASSERT_FALSE_MESSAGE(audio_is_ready(), "Audio should not be ready after shutdown");
}

static void test_ese_audio_volume_control(void) {
    TEST_ASSERT_TRUE_MESSAGE(audio_startup(), "Audio startup should succeed");

    /* Volume set/get */
    audio_set_volume(0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, audio_get_volume());

    /* Clamping */
    audio_set_volume(-0.1f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, audio_get_volume());

    audio_set_volume(1.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, audio_get_volume());
}

static void test_ese_audio_receiver_position(void) {
    TEST_ASSERT_TRUE_MESSAGE(audio_startup(), "Audio startup should succeed");

    /* Set/get receiver position */
    audio_set_receiver(10.0f, 20.0f);

    float x, y;
    audio_get_receiver(&x, &y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, y);

    /* NULL pointers should be safe */
    audio_get_receiver(NULL, &y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, y);

    audio_get_receiver(&x, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, x);
}

static void test_ese_audio_sound_creation(void) {
    TEST_ASSERT_TRUE_MESSAGE(audio_startup(), "Audio startup should succeed");

    /* Valid data */
    EseSound *sound = audio_sound_create(test_audio_data);
    TEST_ASSERT_NOT_NULL_MESSAGE(sound, "Sound creation should succeed");

    /* Defaults */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, audio_sound_get_volume(sound));
    TEST_ASSERT_FALSE_MESSAGE(audio_sound_get_repeat(sound), "Default repeat should be false");
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, audio_sound_get_max_distance(sound));
    TEST_ASSERT_EQUAL_INT_MESSAGE(AUDIO_ATTENUATION_LINEAR, audio_sound_get_attenuation(sound), "Default attenuation should be linear");

    audio_sound_destroy(sound);

    /* NULL data */
    EseSound *null_sound = audio_sound_create(NULL);
    TEST_ASSERT_NULL_MESSAGE(null_sound, "Sound creation with NULL data should fail");
}

static void test_ese_audio_sound_properties(void) {
    TEST_ASSERT_TRUE_MESSAGE(audio_startup(), "Audio startup should succeed");

    EseSound *sound = audio_sound_create(test_audio_data);
    TEST_ASSERT_NOT_NULL_MESSAGE(sound, "Sound creation should succeed");

    /* Volume setting and clamping */
    audio_sound_set_volume(sound, 0.7f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.7f, audio_sound_get_volume(sound));

    audio_sound_set_volume(sound, -0.1f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, audio_sound_get_volume(sound));

    audio_sound_set_volume(sound, 1.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, audio_sound_get_volume(sound));

    /* Repeat */
    audio_sound_set_repeat(sound, true);
    TEST_ASSERT_TRUE_MESSAGE(audio_sound_get_repeat(sound), "Sound repeat should be true");
    audio_sound_set_repeat(sound, false);
    TEST_ASSERT_FALSE_MESSAGE(audio_sound_get_repeat(sound), "Sound repeat should be false");

    /* Position */
    audio_sound_set_position(sound, 5.0f, 10.0f);
    float x, y;
    audio_sound_get_position(sound, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, y);

    /* Max distance and attenuation */
    audio_sound_set_max_distance(sound, 50.0f, AUDIO_ATTENUATION_EXPONENTIAL);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, audio_sound_get_max_distance(sound));
    TEST_ASSERT_EQUAL_INT_MESSAGE(AUDIO_ATTENUATION_EXPONENTIAL, audio_sound_get_attenuation(sound), "Attenuation should be exponential");

    /* Max distance clamping */
    audio_sound_set_max_distance(sound, -10.0f, AUDIO_ATTENUATION_LINEAR);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.1f, audio_sound_get_max_distance(sound));

    audio_sound_destroy(sound);
}

static void test_ese_audio_sound_playback(void) {
    TEST_ASSERT_TRUE_MESSAGE(audio_startup(), "Audio startup should succeed");

    EseSound *sound = audio_sound_create(test_audio_data);
    TEST_ASSERT_NOT_NULL_MESSAGE(sound, "Sound creation should succeed");

    /* Playback controls */
    audio_sound_play(sound);
    audio_sound_pause(sound);
    audio_sound_play(sound);
    audio_sound_stop(sound);

    /* Seek and fade */
    audio_sound_seek(sound, 1000);
    audio_sound_fade(sound, 0.5f, 1.0f);

    audio_sound_destroy(sound);
}

static void test_ese_audio_null_safety(void) {
    TEST_ASSERT_TRUE_MESSAGE(audio_startup(), "Audio startup should succeed");

    /* NULL sound pointer safety */
    audio_sound_destroy(NULL);
    audio_sound_set_volume(NULL, 0.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, audio_sound_get_volume(NULL));

    audio_sound_set_repeat(NULL, true);
    TEST_ASSERT_FALSE_MESSAGE(audio_sound_get_repeat(NULL), "Repeat of NULL sound should be false");

    audio_sound_set_position(NULL, 1.0f, 2.0f);
    float x, y;
    audio_sound_get_position(NULL, &x, &y);

    audio_sound_set_max_distance(NULL, 10.0f, AUDIO_ATTENUATION_LINEAR);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, audio_sound_get_max_distance(NULL));
    TEST_ASSERT_EQUAL_INT_MESSAGE(AUDIO_ATTENUATION_LINEAR, audio_sound_get_attenuation(NULL), "Attenuation of NULL sound should be linear");

    audio_sound_play(NULL);
    audio_sound_pause(NULL);
    audio_sound_stop(NULL);
    audio_sound_seek(NULL, 100);
    audio_sound_fade(NULL, 0.5f, 1.0f);

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, audio_sound_get_length(NULL), "Length of NULL sound should be 0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, audio_sound_get_playback_position(NULL), "Playback position of NULL sound should be 0");
}