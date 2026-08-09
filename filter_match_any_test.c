#include "filter.h"
#include <stdio.h>

static int total = 0, failed = 0;

static void check(const char *label, const char *path, const char *const *patterns,
                   bool expected_match, bool expected_error)
{
    total++;
    bool err = false;
    bool got = filter_match_any(path, patterns, &err);
    if (got != expected_match || err != expected_error) {
        failed++;
        printf("FAIL [%s]: match=%d err=%d, attendu match=%d err=%d\n",
               label, got, err, expected_match, expected_error);
    } else {
        printf("ok   [%s]: match=%d err=%d\n", label, got, err);
    }
}

int main(void)
{
    printf("=== Palier 1 : trivial ===\n");
    check("patterns NULL -> jamais de match", "foo.go", NULL, false, false);
    {
        const char *empty[] = {NULL};
        check("patterns tableau vide (juste le NULL terminal)", "foo.go", empty, false, false);
    }

    printf("\n=== Palier 2 : nominal ===\n");
    {
        const char *p[] = {"*.go", NULL};
        check("un seul pattern, match", "foo.go", p, true, false);
        check("un seul pattern, pas de match", "foo.py", p, false, false);
    }

    printf("\n=== Palier 3 : cas limites ===\n");
    {
        const char *p[] = {"*.go", "*.py", "*.c", NULL};
        check("plusieurs patterns, match sur le premier", "foo.go", p, true, false);
        check("plusieurs patterns, match sur le dernier", "foo.c", p, true, false);
        check("plusieurs patterns, aucun match", "foo.rs", p, false, false);
    }

    printf("\n=== Palier 4 : adversarial ===\n");
    {
        const char *p[] = {"*.go", "[invalid", NULL};
        check("pattern invalide APRES un match reussi -> le match court-circuite avant l'erreur", "foo.go", p, true, false);
    }
    {
        const char *p[] = {"[invalid", "*.go", NULL};
        check("pattern invalide AVANT un match potentiel -> erreur immediate, pas de suite", "foo.go", p, false, true);
    }
    {
        const char *p[] = {"[invalid", NULL};
        check("un seul pattern invalide", "foo.go", p, false, true);
    }

    printf("\n=== Palier 5 : integration (vrai comportement MatchAny de moddwatch) ===\n");
    {
        // Reproduit exactement l'appel interne de filter_file/filter_files
        const char *excludes[] = {"*.tmp", "**/.git/**", NULL};
        check("exclusion typique modd.conf", ".git/config", excludes, true, false);
        check("fichier normal non exclu", "main.go", excludes, false, false);
    }

    printf("\n%d/%d tests OK\n", total - failed, total);
    return failed ? 1 : 0;
}