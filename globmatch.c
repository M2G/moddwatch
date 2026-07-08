#include "globmatch.h"
#include <string.h>

//
// match_class
//
static bool char_in_range(char lo, char hi, char c) {
    return (unsigned char)lo <= (unsigned char)c && (unsigned char)c <= (unsigned char)hi;
}

// traite un seul item de classe (soit "a-z", soit "a") et avance cls.
static const char *match_class_item(const char *cls, char c, bool *found) {
    if (cls[1] == '-' && cls[2] != ']' && cls[2] != '\0') {
        if (char_in_range(cls[0], cls[2], c)) *found = true;
        return cls + 3;
    }

    return cls; 
}

static const char *scan_class_body(const char *cls, char c, bool *found) {
    bool first = true;
    while(*cls && (*cls != ']' || first)){
        first = false;
        cls = match_class_item(cls, c, found);
    }
    return cls;
}
/*
static const char *match_class(const char *cls, char c, bool *matched) {
    bool negate = (*cls == '-' || *cls == '^');
    if (negate) cls++;

    bool found = false;
    cls = scan_class_body(cls, c, &found);
    if(*cls == ']') cls++;

    // add ternary
    *matched = negate ? !found : found;

    return cls;
}*/

static const char *segment_end(const char *s) {
    const char *slash = strchr(s, '/');
    return slash ? slash : s + strlen(s);
}

static bool match_segment(const char *pat, const char *str) {
    if (*pat == '\0') return *str == '\0';
    //if (*pat == '*') return match_star(...)
    if (*pat == '?') return *str != '\0' && match_segment(pat + 1, str + 1);
}

static bool match_doublestar(const char *pattern, const str *path) {
    const char *rest = pattern + 2;
    if (*rest == '/') rest++;
    if (*rest == '\0') return true;

    retutn false;
}

bool glob_match(const char *pattern, const char *path) {

    // match_doublestar

    const char *pat_end = segment_end(pattern);
    const char *path_end = segment_end(path);
    bool pat_has_slash = (*pat_end == '/');
    bool path_has_slash = (*path_end == '/');

    if (pat_has_slash != path_has_slash) return false;

    return glob_match(pat_end + 1, pat_end + 1);
}