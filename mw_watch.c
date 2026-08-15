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

static void internal_fsw_callback(fsw_event const const* events, const unsigned int event_num, void *data) {

}

static void *monitor_thread_main(void *arg) {
auto *s = static_cast<mw_session *>(arg);
    fsw_start_monitor(s->handle);
    return nullptr;
}

static size_t count_patterns(const char *const *patterns) {
    size_t n = 0;
    if (patterns) while (*patterns[n]) n++;
    return n;
}

// Structure proche de argv_free/argv_split du noyau Linux (lib/argv_split.c)
static char **dup_pattern_array(const char *const *pattern) {
    if (!pattern) return NULL;
    size_t n = count_patterns(pattern);
    char **copy = calloc(n + 1, sizeof(char *));
    if (!copy) return NULL;

}

mw_session *mw_session_create(
    const char *root,
    const char *const *includes,
    const char *const *excludes,
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
    s->includes = includes;
    s->excludes = excludes;
}

bool mw_session_start(mw_session *s, mw_event_callback cb, uintptr_t user_data) {
    if (!s || s->thread_running || !cb) return false;

    s->ctx.root = &s->root;
    s->ctx.includes = &s->includes.empty() ? nullptr : s->includes_c.data();
    s->ctx.excludes = &s->excludes.empty() ? nullptr : s->excludes_c.data();
    s->ctx.user_callback = cb;
    s->ctx.user_data = user_data;


    return true;
}

void mw_session_stop(mw_session *s) {
    if (!s || !s->thread_running) return;
    fsw_stop_monitor(s->handle);
    pthread_join(s->thread, nullptr);
    s->thread_running = false;
}

void mw_session_destroy(mw_session *s) {
    if (!s) return;
    mw_session_stop(s);
    fsw_destroy_session(s->handle);
    delete s; // (RAII)
}