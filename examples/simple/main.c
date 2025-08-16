// main.c
#include <execinfo.h>
#include <mach/mach_time.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include "core/engine.h"
#include "platform/renderer.h"
#include "platform/window.h"

void segfault_handler(int signo, siginfo_t *info, void *context) {
    void *buffer[32];
    int nptrs = backtrace(buffer, 32);
    char **strings = backtrace_symbols(buffer, nptrs);
    if (strings) {
        fprintf(stderr, "---- BACKTRACE START ----\n");
        for (int i = 0; i < nptrs; i++) {
            fprintf(stderr, "%s\n", strings[i]);
        }
        fprintf(stderr, "---- BACKTRACE  END  ----\n");
        free(strings);
    }

    signal(signo, SIG_DFL);
    raise(signo);
}

int main(int argc, char *argv[]) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_sigaction = segfault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("Error setting SIGSEGV handler");
        return EXIT_FAILURE;
    }

    EseWindow *window = window_create(800, 600, "Simple Test");
    EseRenderer *renderer = renderer_create(false);
    EseEngine *engine = engine_create("startup.lua");
    window_set_renderer(window, renderer);
    engine_set_renderer(engine, renderer);
    engine_start(engine);

    // Time setup
    uint64_t prev_time = mach_absolute_time();
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);

    EseInputState input_state;
    while(!window_should_close(window)) {
        // Calculate delta time in seconds
        uint64_t now = mach_absolute_time();
        double delta = (double)(now - prev_time) * (double)timebase.numer /
                        (double)timebase.denom / 1e9;
        prev_time = now;

        window_process(window, &input_state);
        engine_update(engine, (float)delta, &input_state);

        if (input_state.keys_pressed[InputKey_ESCAPE]) {
            printf("exit\n");
            window_close(window);
        }
    }
    return 0;
}