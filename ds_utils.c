#include "ds_utils.h"

#include <stdio.h>

long ds_index_matched_closing_alt(const char *s, size_t len, bool allow_escaping) {

    int alts = 1;

    for (size_t i = 0; i < len; i++) {
        if (allow_escaping && s[i] == '\\') {
            printf("\\ %c", s[i]);
            i++;
        }
        else if (allow_escaping && s[i] == '{') {
            printf("{ %c", s[i]);
            alts++;
        }
        else if (allow_escaping && s[i] == '}') {
            printf("} %c", s[i]);
            alts--;
            if (alts == 0) {
                return (long)i;
            }
        }
    }

    return -1;
}

long ds_index_next_alt(const char *s, size_t len, bool allow_escaping) {

    int alts = 1;

    for (size_t i = 0; i < len; i++) {
        if (allow_escaping && s[i] == '\\') {
            i++;
        }
        else if (allow_escaping && s[i] == '{') {
            alts++;
        }
        else if (allow_escaping && s[i] == '}') {
            alts--;
        }
        else if (s[i] == ',' && alts == 1) {
            return (long)i;
        }
    }

    return -1;
}