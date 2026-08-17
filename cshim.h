#include <stdint.h>
#ifndef CSHIM_H
#define CSHIM_H

#include "mw_watch.h"

void mw_go_bridge(const char *path, bool created, bool updated, bool removed, bool renamed, uintptr_t user_data);

#endif