// main.c
#include <execinfo.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "core/engine.h"
#include "platform/renderer.h"
#include "platform/window.h"
#include "platform/time.h"
#include "core/memory_manager.h"
#include "utility/profile.h"


int main(int argc, char *argv[]) {
    int max_time_seconds = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--max-time") == 0 && i + 1 < argc) {
            max_time_seconds = atoi(argv[i + 1]);
            i++; // Skip the next argument since we consumed it
        } else if (strcmp(argv[i], "--enable-all-logs") == 0) {
            // Enable all logs
            setenv("LOG_CATEGORIES", "ALL", 1);
        }
    }

    EseWindow *window = window_create(800, 600, "Bounce");
    EseRenderer *renderer = renderer_create(false);
    EseEngine *engine = engine_create("startup.lua");
    window_set_renderer(window, renderer);
    engine_set_renderer(engine, renderer);
    engine_start(engine);

    // Time setup
    uint64_t prev_time = time_now();
    uint32_t timebase_numer, timebase_denom;
    time_get_conversion_factor(&timebase_numer, &timebase_denom);

    double total_time_seconds = 0;
    double updates_per_second_average = 0;

    EseInputState input_state;
    int snap = 0;
    while(!window_should_close(window)) {
        // Calculate delta time in seconds
        uint64_t now = time_now();
        double delta = (double)(now - prev_time) * (double)timebase_numer /
                        (double)timebase_denom / 1e9;
        prev_time = now;

        total_time_seconds += delta;
        updates_per_second_average += 1 / delta;
        updates_per_second_average /= 2;

        window_process(window, &input_state);
        engine_update(engine, (float)delta, &input_state);

        if (input_state.keys_pressed[InputKey_ESCAPE]) {
            printf("exit\n");
            window_close(window);
        }

        if (max_time_seconds != -1 && total_time_seconds > max_time_seconds) {
            printf("Max time reached\n");
            window_close(window);
        }
    }


    engine_set_renderer(engine, NULL);
    window_set_renderer(window, NULL);

    engine_destroy(engine);
    renderer_destroy(renderer);
    window_destroy(window);

    memory_manager.destroy();

    profile_display();

    printf("\n\nUpdates per second average: %f\n", updates_per_second_average); 
    printf("Total time: %f seconds\n", total_time_seconds);
    printf("\n\nBye\n");
    return 0;
}