#include "ds_match.h"
#include <stdio.h>

static int total = 0, failed = 0;

static void check(const char *pattern, const char *name, ds_result expected)
{
    total++;
    ds_result got = ds_match(pattern, name);
    if (got != expected) {
        failed++;
        printf("FAIL ds_match(\"%s\", \"%s\") = %d, attendu %d\n", pattern, name, got, expected);
    } else {
        printf("ok   ds_match(\"%s\", \"%s\")\n", pattern, name);
    }
}

int main()
{
    // litteral, *, ?, [classe]
    check("main.c", "main.c", DS_MATCH);
    check("main.c", "main.h", DS_NO_MATCH);
    check("*.c", "main.c", DS_MATCH);
    check("*.c", "src/main.c", DS_NO_MATCH);
    check("main.?", "main.c", DS_MATCH);
    check("main.[ch]", "main.h", DS_MATCH);
    check("main.[ch]", "main.x", DS_NO_MATCH);
    check("main.[!a-c]", "main.d", DS_MATCH);

    // segments multiples
    check("src/*.c", "src/main.c", DS_MATCH);
    check("src/*.c", "src/sub/main.c", DS_NO_MATCH);

    // **
    check("**/main.c", "src/sub/main.c", DS_MATCH);
    check("src/**", "src", DS_MATCH);
    check("**/.git/**", "src/.git/config", DS_MATCH);
    check("**.tmp", "foo.tmp", DS_MATCH);
    check("**.tmp", "src/foo.tmp", DS_NO_MATCH); // ** mid-pattern ne traverse pas '/'

    // {a,b,c}
    check("main.{c,h}", "main.c", DS_MATCH);
    check("main.{c,h}", "main.x", DS_NO_MATCH);
    check("some{thing,}", "some", DS_MATCH); // alternative vide

    // pattern invalide
    check("main.[c", "main.[c", DS_BAD_PATTERN);

    printf("\n%d/%d tests OK\n", total - failed, total);
    return failed ? 1 : 0;
}