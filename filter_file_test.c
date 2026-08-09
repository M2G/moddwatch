#include "filter.h"
#include <stdio.h>
#include <string.h>

static int total = 0, failed = 0;

static void check(const char *label, const char *path, const char *const *includes,
                   const char *const *excludes, const char *expected_result, bool expected_error)
{
    total++;
    bool err = false;
    const char *got = filter_file(path, includes, excludes, &err);
    bool result_ok = (got == NULL && expected_result == NULL) ||
                      (got != NULL && expected_result != NULL && strcmp(got, expected_result) == 0);
    if (!result_ok || err != expected_error) {
        failed++;
        printf("FAIL [%s]: result=\"%s\" err=%d, attendu result=\"%s\" err=%d\n",
               label, got ? got : "(NULL)", err, expected_result ? expected_result : "(NULL)", expected_error);
    } else {
        printf("ok   [%s]: result=\"%s\" err=%d\n", label, got ? got : "(NULL)", err);
    }
}

int main(void)
{
    printf("=== Palier 1 : trivial ===\n");
    check("includes et excludes NULL -> rien n'est inclus", "main.go", NULL, NULL, NULL, false);

    printf("\n=== Palier 2 : nominal ===\n");
    {
        const char *inc[] = {"*.go", NULL};
        check("inclus, pas exclu -> accepte", "main.go", inc, NULL, "main.go", false);
        check("pas inclus -> rejete", "main.py", inc, NULL, NULL, false);
    }

    printf("\n=== Palier 3 : cas limites (priorite exclude sur include) ===\n");
    {
        const char *inc[] = {"*"};
        const char *exc[] = {"*.tmp", NULL};
        const char *incAll[] = {"*", NULL};
        check("inclus par '*', pas exclu", "main.go", incAll, exc, "main.go", false);
        check("inclus par '*' MAIS exclu -> rejete (exclude gagne)", "main.tmp", incAll, exc, NULL, false);
        (void)inc;
    }

    printf("\n=== Palier 4 : adversarial ===\n");
    {
        const char *incBad[] = {"[bad", NULL};
        check("pattern d'include invalide -> erreur, pas de resultat", "main.go", incBad, NULL, NULL, true);
    }
    {
        const char *excBad[] = {"[bad", NULL};
        const char *inc[] = {"*.go", NULL};
        check("pattern d'exclude invalide -> erreur (verifie avant include)", "main.go", inc, excBad, NULL, true);
    }
    {
        // exclude invalide mais le fichier aurait de toute facon ete exclu par un pattern VALIDE precedent
        const char *excMixed[] = {"*.go", "[bad", NULL};
        const char *inc[] = {"*", NULL};
        check("exclude valide matche avant d'atteindre le pattern invalide -> pas d'erreur", "main.go", inc, excMixed, NULL, false);
    }

    printf("\n=== Palier 5 : integration (vrai cas filterFilesTests) ===\n");
    {
        // filterFilesTests[3] : includes=["main.*"], excludes=["*.cpp"]
        const char *inc[] = {"main.*", NULL};
        const char *exc[] = {"*.cpp", NULL};
        check("main.cpp exclu malgre include", "main.cpp", inc, exc, NULL, false);
        check("main.go inclus, pas exclu", "main.go", inc, exc, "main.go", false);
        check("foo.go ni inclus ni exclu -> rejete", "foo.go", inc, exc, NULL, false);
    }

    printf("\n%d/%d tests OK\n", total - failed, total);
    return failed ? 1 : 0;
}