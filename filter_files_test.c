#include "filter.h"
#include <stdio.h>
#include <string.h>

static int total = 0, failed = 0;

static void check_files(const char *label, const char *const *files, size_t n_files,
                         const char *const *includes, const char *const *excludes,
                         size_t out_cap, long expected_n, const char *const *expected)
{
    total++;
    const char *out[16];
    long n = filter_files(files, n_files, includes, excludes, out, out_cap);

    bool ok = (n == expected_n);
    if (ok) {
        for (long i = 0; i < n; i++) {
            if (strcmp(out[i], expected[i]) != 0) { ok = false; break; }
        }
    }
    if (!ok) {
        failed++;
        printf("FAIL [%s]: n=%ld, attendu %ld\n", label, n, expected_n);
    } else {
        printf("ok   [%s]: n=%ld\n", label, n);
    }
}

int main(void)
{
    printf("=== Palier 1 : trivial ===\n");
    {
        const char *files[] = {NULL};
        check_files("aucun fichier", files, 0, NULL, NULL, 16, 0, NULL);
    }

    printf("\n=== Palier 2 : nominal ===\n");
    {
        const char *files[] = {"a.go", "b.py"};
        const char *inc[] = {"*.go", NULL};
        const char *expected[] = {"a.go"};
        check_files("un seul retenu sur deux", files, 2, inc, NULL, 16, 1, expected);
    }

    printf("\n=== Palier 3 : cas limites ===\n");
    {
        const char *files[] = {"a.go", "b.go"};
        const char *inc[] = {"*.go", NULL};
        const char *expected[] = {"a.go", "b.go"};
        check_files("tous retenus", files, 2, inc, NULL, 16, 2, expected);
    }
    {
        const char *files[] = {"a.py", "b.py"};
        const char *inc[] = {"*.go", NULL};
        check_files("aucun retenu", files, 2, inc, NULL, 16, 0, NULL);
    }

    printf("\n=== Palier 4 : adversarial ===\n");
    {
        const char *files[] = {"a.go", "b.go", "c.go"};
        const char *inc[] = {"*.go", NULL};
        long n = filter_files(files, 3, inc, NULL, (const char **)0, 0);
        total++;
        // capacite 0 mais 3 resultats attendus -> doit signaler l'insuffisance (-1)
        if (n == -1) { printf("ok   [capacite de sortie insuffisante -> -1]: n=%ld\n", n); }
        else { failed++; printf("FAIL [capacite de sortie insuffisante -> -1]: n=%ld, attendu -1\n", n); }
    }
    {
        // Un pattern invalide au milieu de la liste : ce FICHIER est saute
        // (continue), pas d'arret global - fidelite au vrai Files() Go
        const char *files[] = {"a.go", "b.go"};
        const char *inc[] = {"*.go", NULL};
        const char *excBad[] = {"[bad", NULL};
        check_files("pattern exclude invalide -> le fichier concerne est saute, pas d'arret global",
                    files, 2, inc, excBad, 16, 0, NULL);
    }

    printf("\n=== Palier 5 : integration (vrais filterFilesTests, deja couverts par test_filter.c) ===\n");
    {
        const char *files[] = {"main.cpp", "main.go", "main.h", "foo.go", "bar.py"};
        const char *inc[] = {"*", NULL};
        const char *exc[] = {"*.go", NULL};
        const char *expected[] = {"main.cpp", "main.h", "bar.py"};
        check_files("filterFilesTests[2]", files, 5, inc, exc, 16, 3, expected);
    }

    printf("\n%d/%d tests OK\n", total - failed, total);
    return failed ? 1 : 0;
}