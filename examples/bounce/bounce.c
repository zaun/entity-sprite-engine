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


int main(int argc, char *argv[]) {

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

    EseInputState input_state;
    while(!window_should_close(window)) {
        // Calculate delta time in seconds
        uint64_t now = time_now();
        double delta = (double)(now - prev_time) * (double)timebase_numer /
                        (double)timebase_denom / 1e9;
        prev_time = now;

        window_process(window, &input_state);
        engine_update(engine, (float)delta, &input_state);

        if (input_state.keys_pressed[InputKey_ESCAPE]) {
            printf("exit\n");
            window_close(window);
        }
    }


    engine_set_renderer(engine, NULL);
    window_set_renderer(window, NULL);

    engine_print_stats(engine);

    engine_destroy(engine);
    renderer_destroy(renderer);
    window_destroy(window);

    memory_manager.destroy();

    printf("Bye\n");
    return 0;
}