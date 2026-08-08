#include "ds_match.h"
#include <stdio.h>

static int total = 0, failed = 0;

static void check(const char *label, const char *pattern, const char *name, ds_result expected)
{
    total++;
    ds_result got = ds_match(pattern, name);
    const char *labels[] = {"NO_MATCH", "MATCH"};
    const char *g = (got == DS_BAD_PATTERN) ? "BAD_PATTERN" : labels[got];
    const char *e = (expected == DS_BAD_PATTERN) ? "BAD_PATTERN" : labels[expected];
    if (got != expected) {
        failed++;
        printf("FAIL [%s]: ds_match(\"%s\", \"%s\") = %s, attendu %s\n", label, pattern, name, g, e);
    } else {
        printf("ok   [%s]: -> %s\n", label, g);
    }
}

int main(void)
{
    printf("=== Palier 1 : trivial ===\n");
    check("vide contre vide", "", "", DS_MATCH);
    check("pattern vide, nom non-vide", "", "x", DS_NO_MATCH);
    check("pattern non-vide, nom vide", "x", "", DS_NO_MATCH);

    printf("\n=== Palier 2 : nominal (litteral, *, ?) ===\n");
    check("litteral identique", "main.c", "main.c", DS_MATCH);
    check("litteral different", "main.c", "main.h", DS_NO_MATCH);
    check("etoile simple", "*.c", "main.c", DS_MATCH);
    check("point d'interrogation", "main.?", "main.c", DS_MATCH);

    printf("\n=== Palier 3 : cas limites (segments, **, classes) ===\n");
    check("etoile ne traverse pas '/'", "*.c", "src/main.c", DS_NO_MATCH);
    check("segments multiples exacts", "src/main.c", "src/main.c", DS_MATCH);
    check("** aligne-segment, zero segment", "**/main.c", "main.c", DS_MATCH);
    check("** aligne-segment, plusieurs segments", "**/main.c", "src/sub/main.c", DS_MATCH);
    check("** en fin, matche le dossier lui-meme (UPGRADING.md)", "src/**", "src", DS_MATCH);
    check("classe simple", "main.[ch]", "main.h", DS_MATCH);
    check("classe negatee", "main.[!a-c]", "main.d", DS_MATCH);
    check("alternative simple", "main.{c,h}", "main.h", DS_MATCH);

    printf("\n=== Palier 4 : adversarial ===\n");
    check("** mid-pattern degrade en simple star (doc officielle)", "**.tmp", "foo.tmp", DS_MATCH);
    check("** mid-pattern NE traverse PAS '/' (contre-intuitif mais confirme)", "**.tmp", "src/foo.tmp", DS_NO_MATCH);
    check("classe non fermee -> erreur explicite (pas de fallback litteral)", "main.[c", "main.[c", DS_BAD_PATTERN);
    check("classe vide malformee (']' consomme comme litteral 1ere position -> pas de fermeture reelle)", "[]", "x", DS_BAD_PATTERN);
    check("alternative vide -> matche l'absence", "some{thing,}", "some", DS_MATCH);
    check("etoiles consecutives fusionnees", "***.c", "main.c", DS_MATCH);
    check("chemin absolu vs pattern relatif", "/main.c", "main.c", DS_NO_MATCH);
    check("segment vide via '/' en tete", "/main.c", "/main.c", DS_MATCH);
    check("** imbrique dans {} + slash", "{**/main.c,other}", "src/main.c", DS_MATCH);

    printf("\n=== Palier 5 : integration (vrais patterns filterFilesTests moddwatch) ===\n");
    check("filterFilesTests: *", "*", "main.cpp", DS_MATCH);
    check("filterFilesTests: *.go", "*.go", "main.go", DS_MATCH);
    check("filterFilesTests: *.go non-match", "*.go", "main.cpp", DS_NO_MATCH);
    check("filterFilesTests: main.*", "main.*", "main.cpp", DS_MATCH);
    check("filterFilesTests: **/* sur chemin absolu", "**/*", "/test/foo.go", DS_MATCH);
    check("filterFilesTests: **/* sur nom simple", "**/*", "foo", DS_MATCH);

    printf("\n%d/%d tests OK\n", total - failed, total);
    return failed ? 1 : 0;
}