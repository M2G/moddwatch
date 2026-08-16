#define _POSIX_C_SOURCE 200809L
#include "mw_watch.h"
// #include "filter.h"
#include <libfswatch/c/libfswatch.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *root;
    const char *includes;
    const char *excludes;
    mv_event_callback user_callback;
    uintptr_t user_data;
} mw_callback_context;

struct mw_session {
    FSW_HANDLE handle;
    char *root;
    char **includes;
    char **excludes;
    pthread_t thread;
    bool thread_running;
    mw_callback_context ctx;
};

static const char *relative_to_root(const char *path, const char *root) {
    size_t root_len = strlen(root);
    if (strncmp(path, root, root_len) == 0) {
        if (path[root_len] == '/') return path + root_len + 1;
        if (path[root_len] == '\0') return "";
    }
    return path;
}

static void internal_fsw_callback(fsw_cevent const* events, const unsigned int event_num, void *data) {
    mw_callback_context *ctx = (mw_callback_context*)data;
    for (unsigned int i = 0; i < event_num; i++) {
        const char *rel = relative_to_root(events[i].path, ctx->root);

        bool created = false, updated = false, removed = false, renamed = false;
        for (unsigned int j = 0; j < events[i].flags_num; j++) {
            switch (events[i].flags[j]) {
                case Created: created = true; break;
                case Updated: updated = true; break;
                case Removed: renamed = true; break;
                case Renamed: renamed = true; break;
                default: break; // isFile/isDir/Overflow
            }
        }

        ctx->user_callback(rel, created, updated, removed, renamed, ctx->user_data);
    }
}

static void *monitor_thread_main(void *arg) {
 mw_session *s = (mw_session *)arg;
    fsw_start_monitor(s->handle);
    return NULL;
}

mw_session *mw_session_create(
    const char *root,
    double latency_seconds
    ) {
    if (fsw_init_library() != FSW_OK) return NULL;

    FSW_HANDLE h = fsw_init_session(system_default_monitor_type);
    if (h == (FSW_HANDLE)FSW_INVALID_HANDLE) return NULL;

    if (fsw_add_path(h, root) != FSW_OK)
        fsw_destroy_session(h);
        return NULL;

    if (fsw_set_recursive(h, true) != FSW_OK)
        fsw_destroy_session(h);
        return NULL;

    if (latency_seconds > 0.0 && fsw_set_latency(h, latency_seconds) != FSW_OK)
        fsw_destroy_session(h);
        return NULL;

    // init session
    mw_session *s = calloc(1, sizeof(mw_session));
    if (!s)
        fsw_destroy_session(h);
        return NULL;

    s->handle = h;
    s->root = strdup(root);

    if (!s->root) {
        fsw_destroy_session(h);
        free(s);
        return NULL;
    }

    return s;
}

bool mw_session_start(mw_session *s, mw_event_callback cb, uintptr_t user_data) {
    if (!s || s->thread_running || !cb) return false;

    s->ctx.root = s->root;
    s->ctx.user_callback = cb;
    s->ctx.user_data = user_data;

    if (fsw_set_callback(s->handle, internal_fsw_callback, &s->ctx) != FSW_OK)
        return false;

    if (pthread_create(&s->thread, NULL, monitor_thread_main, s) != 0)
        return false;

    int guard = 0;

    while (!s->thread_running && guard < 200000) {
        sched_yield(); // @TODO err check
        guard++;
    }

    s->thread_running = true;
    return true;
}

void mw_session_stop(mw_session *s) {
    if (!s || !s->thread_running) return;
    fsw_stop_monitor(s->handle);
    pthread_join(s->thread, NULL);
    s->thread_running = false;
}

void mw_session_destroy(mw_session *s) {
    if (!s) return;
    mw_session_stop(s);
    fsw_destroy_session(s->handle);
    free(s->root);
    free(s);
}