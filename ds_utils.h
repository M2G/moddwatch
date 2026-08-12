#ifndef DS_UTILS_H
#define DS_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

long ds_index_matched_closing_alt(const char *s, size_t len, bool allow_escaping);

long ds_index_next_alt(const char *s, size_t len, bool allow_escaping);

bool ds_validate_pattern(const char *s, size_t len, char separator);

#ifdef __cplusplus
}
#endif

#endif