#pragma once

#include <stdint.h>

typedef void* MODDWATCH_HANDLE;

MODDWATCH_HANDLE moddwatch_create();

int moddwatch_add(
    MODDWATCH_HANDLE handle,
    const char* path
);

int moddwatch_start(
    MODDWATCH_HANDLE handle
);

int moddwatch_next(
    MODDWATCH_HANDLE handle,
    char* path,
    uint32_t* flags
);

void moddwatch_destroy(
    MODDWATCH_HANDLE handle
);