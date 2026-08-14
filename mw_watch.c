/*#define _POSIX_C_SOURCE 200809L
#include "mw_watch.h"
// #include "filter.h"
#include <libfswatch/c/libfswatch.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mw_callback_context {
    const std::string *root = nullptr;
    const char *const *includes = nullptr;
    const char *const *excludes = nullptr;
    mw_event_callback user_callback = nullptr;
    uintptr_t user_data = 0;
};
struct mw_session {
    FSW_HANDLE handle = nullptr;
    std::string root;
    std::vector<std::string> includes;
    std::vector<std::string> excludes;
    std::vector<const char *> include_c;
    std::vector<const char *> exclude_c;
    pthread_t thread{};
    bool thread_running = false;
    mw_callback_context ctx;
};

static void internal_fsw_callback(fsw_event const const* events, const unsigned int event_num, void *data) {

}

static void *monitor_thread_main(void *arg) {
auto *s = static_cast<mw_session *>(arg);
    fsw_start_monitor(s->handle);
    return nullptr;
}

mw_session *mw_session_create(
    const char *root,
    const char *const *includes,
    const char *const *excludes,
    double latency_seconds
    ) {
    if (fsw_init_library() != FSW_OK) return nullptr;
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