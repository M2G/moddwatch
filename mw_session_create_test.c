#include "mw_watch.h"
#include <stdio.h>

static int total = 0, failed = 0;

static void check_created(const char *label, const char *root,
                           const char *const *includes, const char *const *excludes,
                           double latency, bool expect_success)
{
    total++;
    mw_session *s = mw_session_create(root, includes, excludes, latency);
    bool success = (s != NULL);
    if (success != expect_success) {
        failed++;
        printf("FAIL [%s]: session %s, attendu %s\n", label,
               success ? "creee" : "NULL", expect_success ? "creee" : "NULL");
    } else {
        printf("ok   [%s]: session %s\n", label, success ? "creee" : "NULL");
    }
    mw_session_destroy(s);
}

int main(void)
{
    printf("=== Palier 1 : trivial ===\n");
    check_created("chemin minimal valide, pas de patterns, latence par defaut", "/tmp", NULL, NULL, 0.0, true);

    printf("\n=== Palier 2 : nominal ===\n");
    {
        const char *inc[] = {"*.go", NULL};
        const char *exc[] = {"*.tmp", NULL};
        check_created("avec includes/excludes", "/tmp", inc, exc, 0.5, true);
    }

    printf("\n=== Palier 3 : cas limites ===\n");
    check_created("latence negative -> traitee comme defaut, pas d'echec", "/tmp", NULL, NULL, -1.0, true);
    check_created("latence tres grande -> acceptee", "/tmp", NULL, NULL, 3600.0, true);
    {
        const char *inc[] = {NULL}; // tableau non-NULL mais vide (juste le terminateur)
        check_created("includes non-NULL mais vide", "/tmp", inc, NULL, 0.5, true);
    }

    printf("\n=== Palier 4 : adversarial ===\n");
    // Confirme empiriquement plus tot dans la conversation : fsw_add_path
    // ne valide PAS l'existence du chemin -> la session se cree quand meme
    check_created("chemin inexistant -> pas d'echec a la creation (comportement reel fswatch)",
                  "/chemin/totalement/bidon/xyz123", NULL, NULL, 0.5, true);
    check_created("chemin vide", "", NULL, NULL, 0.5, true);

    printf("\n=== Palier 5 : integration (deja couvert par watch_test.go end-to-end) ===\n");
    printf("(TestWatchEndToEnd exerce mw_session_create+start+stop+destroy ensemble,\n");
    printf(" avec de vrais evenements filesystem - pas reteste ici en double)\n");

    printf("\n%d/%d tests OK\n", total - failed, total);
    return failed ? 1 : 0;
}
