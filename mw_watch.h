#include <stdint.h>
#ifndef MW_WATCH_H
#define MW_WATCH_H
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mw_session mw_session;

typedef void (*mv_event_callback)(const char *path, bool is_created, bool is_updated, bool is_removed, bool is_renamed, uintptr_t user_data);

mw_session *mw_session_create(const char *root, const char *const *includes, const char *const *excludes, double latency_seconds);

bool mw_session_start(mw_session *s, mv_event_callback cb, uintptr_t user_data);

void mw_session_stop(mw_session *s);

void mw_session_destroy(mw_session *s);

#ifdef __cplusplus
}
#endif

#endif