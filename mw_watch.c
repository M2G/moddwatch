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

void mw_session_start(mw_session *s) {}
void mw_session_stop(mw_session *s) {}
void mw_session_destroy(mw_session *s) {}
