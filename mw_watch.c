#define _POSIX_C_SOURCE 200809L
#include "mw_watch.h"
#include "filter.h"
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
} mv_callback_context;

struct mw_session {
    FSW_HANDLE handle;
    char *root;
    char **includes;
    char **excludes;
    pthread_t thread;
    bool thread_running;
    mv_callback_context ctx;
};

static size_t count_patterns(const char *const *patterns){
    size_t n = 0;
    if (patterns) while (patterns[n]) n++;
    return n;
}

static char **dup_pattern_array(const char *const *pattern) {
    if (!pattern) return;
    for (size_t i = 0; patterns[i]; i++) free(patterns[i]);
    free(patterns);
}

static void free_pattern_array(char **patterns){}

static const char *relative_to_root(const char *path, const char *root){}

static void internal_fsw_callback(fsw_cevent const *const events, const unsigned int event_num, void *data){}

static void *monitor_thread_main(void *arg){}

void mw_session_start(mw_session *s) {

}
void mw_session_stop(mw_session *s) {
    if (!s || !s->thread_running) return;
    fsw_stop_monitor(s->handle);
    pthread_join(s->thread, NULL);
    s->thread_running = false;
}

void mw_session_destroy(mw_session *s) {

}
