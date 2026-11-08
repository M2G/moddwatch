#ifndef DS_MATCH_H
#define DS_MATCH_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    DS_NO_MATCH   = 0,
    DS_MATCH      = 1,
    DS_BAD_PATTERN = -1
} ds_result;

/* NUL-terminated strings. */
ds_result ds_match(const char *pattern, const char *name);

/* Explicit lengths: the strings need not be NUL-terminated, and no byte past
   pat_len / name_len is ever read. A NUL byte is an ordinary character. */
ds_result ds_match_n(const char *pattern, size_t pat_len, const char *name, size_t name_len);

#endif