#include <stdint.h>
#ifndef MW_WATCH_H
#define MW_WATCH_H
#include <stdbool.h>

typedef struct mw_session mwsession;

typedef void (*mv_event_callback)(const char *path, bool is_created, bool is_updated, bool is_removed, bool is_renamed, uintptr_t user_data);

mwsession *mw_session_create(const char *root, const char *const *includes, const char *const *excludes, double latency_seconds);