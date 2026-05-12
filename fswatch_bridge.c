/*
fswatch_bridge.c
*/

#include "fswatch_bridge.h"

#include <libfswatch/c/libfswatch.h>

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EVENT_QUEUE_SIZE 1024
#define MAX_WATCH_PATHS 128

typedef struct {
    char path[4096];
    uint32_t flags;
} queued_event;

typedef struct {

    FSW_HANDLE monitor;

    queued_event queue[EVENT_QUEUE_SIZE];

    int queue_head;
    int queue_tail;

    pthread_mutex_t mutex;

    char watch_paths[MAX_WATCH_PATHS][4096];
    int watch_path_count;

    int monitor_started;

} moddwatch_handle;

static void fswatch_callback(
    const fsw_cevent *events,
    unsigned int event_num,
    void *data
) {
    moddwatch_handle* h =
        (moddwatch_handle*)data;

    pthread_mutex_lock(&h->mutex);

    for (unsigned int i = 0; i < event_num; i++) {

        int allowed = 0;

        for (int j = 0; j < h->watch_path_count; j++) {

            size_t len =
                strlen(h->watch_paths[j]);

            if (
                strcmp(
                    events[i].path,
                    h->watch_paths[j]
                ) == 0
                ||
                (
                    strncmp(
                        events[i].path,
                        h->watch_paths[j],
                        len
                    ) == 0
                    &&
                    events[i].path[len] == '/'
                )
            ) {
                allowed = 1;
                break;
            }
        }

        if (!allowed) {
            continue;
        }

        int next_tail =
            (h->queue_tail + 1)
            % EVENT_QUEUE_SIZE;

        if (next_tail == h->queue_head) {
            continue;
        }

        strncpy(
            h->queue[h->queue_tail].path,
            events[i].path,
            sizeof(
                h->queue[h->queue_tail].path
            ) - 1
        );

        h->queue[h->queue_tail].path[
            sizeof(
                h->queue[h->queue_tail].path
            ) - 1
        ] = '\0';

        h->queue[h->queue_tail].flags = 1;

        h->queue_tail = next_tail;
    }

    pthread_mutex_unlock(&h->mutex);
}

static void* monitor_thread(void* arg) {

    moddwatch_handle* h =
        (moddwatch_handle*)arg;

    fsw_start_monitor(h->monitor);

    return NULL;
}

MODDWATCH_HANDLE moddwatch_create() {

    moddwatch_handle* h =
        calloc(1, sizeof(moddwatch_handle));

    if (!h) {
        return NULL;
    }

    pthread_mutex_init(
        &h->mutex,
        NULL
    );

    h->monitor =
        fsw_init_session(
            fsevents_monitor_type
        );

    if (!h->monitor) {
        free(h);
        return NULL;
    }

    fsw_set_latency(
        h->monitor,
        0.1
    );

    fsw_set_callback(
        h->monitor,
        fswatch_callback,
        h
    );

    return h;
}

int moddwatch_add(
    MODDWATCH_HANDLE handle,
    const char* path
) {
    moddwatch_handle* h =
        (moddwatch_handle*)handle;

    if (!h) {
        return -1;
    }

    if (
        h->watch_path_count
        >= MAX_WATCH_PATHS
    ) {
        return -1;
    }

    strncpy(
        h->watch_paths[h->watch_path_count],
        path,
        sizeof(
            h->watch_paths[h->watch_path_count]
        ) - 1
    );

    h->watch_paths[h->watch_path_count][
        sizeof(
            h->watch_paths[h->watch_path_count]
        ) - 1
    ] = '\0';

    h->watch_path_count++;

    fsw_add_path(
        h->monitor,
        path
    );

    if (!h->monitor_started) {

        pthread_t tid;

        int tret = pthread_create(
            &tid,
            NULL,
            monitor_thread,
            h
        );

        if (tret != 0) {
            return -1;
        }

        pthread_detach(tid);

        h->monitor_started = 1;
    }

    return 0;
}

int moddwatch_next(
    MODDWATCH_HANDLE handle,
    moddwatch_event* ev
) {
    moddwatch_handle* h =
        (moddwatch_handle*)handle;

    if (!h) {
        return 0;
    }

    pthread_mutex_lock(&h->mutex);

    if (h->queue_head == h->queue_tail) {
        pthread_mutex_unlock(&h->mutex);
        return 0;
    }

    strncpy(
        ev->path,
        h->queue[h->queue_head].path,
        sizeof(ev->path) - 1
    );

    ev->path[
        sizeof(ev->path) - 1
    ] = '\0';

    ev->flags =
        h->queue[h->queue_head].flags;

    h->queue_head =
        (h->queue_head + 1)
        % EVENT_QUEUE_SIZE;

    pthread_mutex_unlock(&h->mutex);

    return 1;
}

void moddwatch_destroy(
    MODDWATCH_HANDLE handle
) {
    moddwatch_handle* h =
        (moddwatch_handle*)handle;

    if (!h) {
        return;
    }

    if (h->monitor) {

        fsw_stop_monitor(h->monitor);

        fsw_destroy_session(
            h->monitor
        );
    }

    pthread_mutex_destroy(
        &h->mutex
    );

    free(h);
}