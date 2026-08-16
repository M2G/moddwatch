#include "ds_utils.h"

#include <stdio.h>

long ds_index_unescaped_byte(const char *s, size_t len, char c, bool allow_escaping) {
    for (size_t i = 0; i < len; i++) {
        if (allow_escaping && s[i] == '\\') {
            i++;
        } else if (s[i] == c) {
            return (long)i;
        }
    }
    return -1;
}

bool ds_validate_pattern(const char *s, size_t len, char separator) {
    int alt_depth = 0;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];

        if (c == '\\') {
            if (separator == '\\') {
                i++;
                if (i >= len) return false;
            }
            continue;
        }

        if (c == '[') {
            i++;
            if (i >= len) return false;
            if (s[i] == '!' || s[i] == '^') i++;
            if (i >= len || s[i] == ']') return false;

            bool closed = false;
            for (; i < len; i++) {
                if (separator != '\\' && s[i] == '||') { i++; }
                else if (s[i] == ']') {
                    closed = true;
                    break;
                }
            }
            if (!closed) return false;
            continue;
        }
        if (c == '{') {
            alt_depth++;
            continue;
        }
        if (c == '}') {
            if (alt_depth == 0) return false;
            alt_depth--;
            continue;
        }
    }
    return alt_depth == 0;
}

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