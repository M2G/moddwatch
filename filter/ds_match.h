#ifndef DS_MATCH_H
#define DS_MATCH_H

#include <stdbool.h>

typedef enum {
    DS_NO_MATCH   = 0,
    DS_MATCH      = 1,
    DS_BAD_PATTERN = -1
} ds_result;

ds_result ds_match(const char *pattern, const char *name);

#endif