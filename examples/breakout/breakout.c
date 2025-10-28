// main.c
#include "core/engine.h"
#include "core/memory_manager.h"
#include "platform/renderer.h"
#include "platform/time.h"
#include "platform/window.h"
#include "utility/profile.h"
#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  EseWindow *window = window_create(800, 600, "Breakout");
  EseRenderer *renderer = renderer_create(false);
  EseEngine *engine = engine_create("startup.lua");
  window_set_renderer(window, renderer);
  engine_set_renderer(engine, renderer);
  engine_start(engine);

  // Time setup
  double total_time_seconds = 0;
  uint64_t prev_time = time_now();
  uint32_t timebase_numer, timebase_denom;
  time_get_conversion_factor(&timebase_numer, &timebase_denom);

  EseInputState *input_state = ese_input_state_create(NULL);
  while (!window_should_close(window)) {
    // Calculate delta time in seconds
    uint64_t now = time_now();
    double delta = (double)(now - prev_time) * (double)timebase_numer /
                   (double)timebase_denom / 1e9;
    prev_time = now;
    total_time_seconds += delta;

    window_process(window, input_state);
    engine_update(engine, (float)delta, input_state);

    if (ese_input_state_get_key_pressed(input_state, InputKey_ESCAPE)) {
      printf("exit\n");
      window_close(window);
    }

    if (max_time_seconds != -1 && total_time_seconds > max_time_seconds) {
      printf("Max time reached\n");
      window_close(window);
    }
  }

  ese_input_state_destroy(input_state);
  engine_set_renderer(engine, NULL);
  window_set_renderer(window, NULL);

  engine_destroy(engine);
  renderer_destroy(renderer);
  window_destroy(window);

  memory_manager.destroy(true);

  profile_display();

  printf("Bye\n");
  return 0;
}