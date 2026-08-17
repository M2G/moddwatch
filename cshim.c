#include "cshim.h"
#include "_cgo_export.h"

void mw_go_bridge(const char *path, bool created, bool updated, bool removed, bool renamed, uintptr_t user_data) {
    goEventTrampoline((char *)path, created ? 1 : 0, updated ? 1 : 0, removed ? 1 : 0, renamed ? 1 : 0, user_data);
}