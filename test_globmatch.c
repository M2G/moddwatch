#include "globmatch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int total = 0;
static int failed = 0;

static void check(const char *pattern, const char *path, bool expected)
{
    total++;
    bool got = glob_match(pattern, path);
    if (got != expected) {
        failed++;
        printf("FAIL: glob_match(\"%s\", \"%s\") = %s, attendu %s\n",
               pattern, path, got ? "true" : "false", expected ? "true" : "false");
    } else {
        printf("ok:   glob_match(\"%s\", \"%s\") = %s\n",
               pattern, path, got ? "true" : "false");
    }
}

int main(void)
{
    // littéral
    check("main.c", "main.c", true);
    check("main.c", "main.h", false);

    // '*' (un seul segment)
    check("*.c", "main.c", true);
    check("*.c", "src/main.c", false); // ne traverse pas '/'
    check("main.*", "main.c", true);
    check("*", "main.c", true);
    check("*", "", true); // matche la chaine vide
 
    // '?'
    check("main.?", "main.c", true);
    check("main.?", "main.cc", false);
    check("?ain.c", "main.c", true);
 
    //'[classe]'
    check("main.[ch]", "main.c", true);
    check("main.[ch]", "main.h", true);
    check("main.[ch]", "main.x", false);
    check("main.[a-c]", "main.b", true);
    check("main.[a-c]", "main.d", false);
    check("main.[!a-c]", "main.d", true);
    check("main.[!a-c]", "main.b", false);
    check("*.py[cod]", "foo.pyc", true);
    check("*.py[cod]", "foo.pyx", false);

    printf("\n%d/%d tests OK\n", total - failed, total);
    return failed ? 1 : 0;
}