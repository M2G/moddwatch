#include "fswatch_bridge.h"

#include <libfswatch/c/libfswatch.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static FSW_HANDLE monitor;

static char last_path[4096];
static uint32_t last_flags = 0;
static int has_event = 0;


void* monitor_thread(void* arg) {
    printf("THREAD STARTED\n");
    fflush(stdout);

    int ret = fsw_start_monitor((FSW_HANDLE)arg);

    printf("MONITOR RET: %d\n", ret);
    printf("LAST ERROR: %s\n", fsw_last_error());
    fflush(stdout);

    return NULL;
}

static void fswatch_callback(
    const fsw_cevent *events,
    unsigned int event_num,
    void *data
) {
    if (event_num == 0) {
        return;
    }

    fflush(stdout);

    strncpy(
        last_path,
        events[0].path,
        sizeof(last_path) - 1
    );

    last_flags = 1;

    has_event = 1;
}

int moddwatch_create() {
  monitor = fsw_init_session(
      kqueue_monitor_type
  );

  printf("MONITOR CREATED\n");

    if (!monitor) {
        return -1;
    }

fsw_set_latency(monitor, 0.1);

fsw_set_callback(
    monitor,
    fswatch_callback,
    NULL
);

    return 1;
}

int moddwatch_add(
    int handle,
    const char* path
) {
    printf("WATCH PATH: %s\n", path);
    fflush(stdout);

    fsw_add_path(
        monitor,
        path
    );

    pthread_t tid;

  int tret = pthread_create(
      &tid,
      NULL,
      monitor_thread,
      monitor
  );

  printf("THREAD RET: %d\n", tret);
  fflush(stdout);

    return 0;
}

int moddwatch_next(
    int handle,
    moddwatch_event* ev
) {
    if (!has_event) {
        return 0;
    }

    strncpy(
        ev->path,
        last_path,
        sizeof(ev->path) - 1
    );

    ev->flags = last_flags;

    has_event = 0;

    return 1;
}

void moddwatch_destroy(int handle) {
}