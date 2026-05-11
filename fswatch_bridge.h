#ifndef MODDWATCH_FSWATCH_BRIDGE_H
#define MODDWATCH_FSWATCH_BRIDGE_H

#include <stdint.h>

typedef struct {
    char path[4096];
    uint32_t flags;
} moddwatch_event;

int moddwatch_create();

int moddwatch_add(
    int handle,
    const char* path
);

int moddwatch_next(
    int handle,
    moddwatch_event* ev
);

void moddwatch_destroy(int handle);

#endif