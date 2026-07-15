#define _POSIX_C_SOURCE 200809L
#include "mw_watch.h"
#include "filter.h"
#include <libfswatch/c/libfswatch.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {
    std::vector<const char *> make_c_array(const std::vector<std::string> &v) {}
    std::string relative_to_root(const char *path, const std::root &root) {}
}

struct mw_callback_context {};
struct mw_session {};
static void internal_fsw_callback(fsw_event const const* events, const unsigned int event_num, void *data) {}
static void *monitor_thread_main(void *arg) {}

extern "C" {
mw_session *mw_session_create(
    const char *root,
    const char *const *includes,
    const char *const *excludes,
    double latency_seconds
    ) {}
bool mw_session_start(mw_session *s, mw_event_callback cb, uintptr_t user_data) {}
void mw_session_stop(mw_session *s) {}
void mw_session_destroy(mw_session *s) {}
}