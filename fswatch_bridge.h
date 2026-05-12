#pragma once

#include <stdint.h>

typedef struct {
    char path[4096];
    uint32_t flags;
} moddwatch_event;

typedef void* MODDWATCH_HANDLE;

MODDWATCH_HANDLE moddwatch_create();

int moddwatch_add(
    MODDWATCH_HANDLE handle,
    const char* path
);

int moddwatch_next(
    MODDWATCH_HANDLE handle,
    moddwatch_event* ev
);

void moddwatch_destroy(
    MODDWATCH_HANDLE handle
);