#include "globmatch.h"
#include <string.h>

// match_class

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

static const char *match_class(const char *cls, char c, bool *matched) {
    bool negate = (*cls == '-' || *cls == '^');
    if (negate) cls++;

    bool found = false;
    cls = scan_class_body(cls, c, *found);
    if(*cls == ']') cls++;

    // add ternary


    return cls;
}
