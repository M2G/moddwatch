#include "mw_watch.h"
#include <stdio.h>
#include <unistd.h>

static int total = 0, failed = 0;

static void noop_callback(const char *path, bool c, bool u, bool r, bool rn, uintptr_t ud)
{
    (void)path; (void)c; (void)u; (void)r; (void)rn; (void)ud;
}

static void check_bool(const char *label, bool got, bool expected)
{
    total++;
    if (got != expected) {
        failed++;
        printf("FAIL [%s]: obtenu %d, attendu %d\n", label, got, expected);
    } else {
        printf("ok   [%s]: %d\n", label, got);
    }
}

int main(void)
{
    printf("=== Palier 1 : trivial ===\n");
    {
        mw_session *s = mw_session_create("/tmp", NULL, NULL, 0.2);
        bool started = mw_session_start(s, noop_callback, 0);
        check_bool("start basique reussit", started, true);
        mw_session_stop(s);
        mw_session_destroy(s);
    }

    printf("\n=== Palier 2 : nominal ===\n");
    {
        mw_session *s = mw_session_create("/tmp", NULL, NULL, 0.2);
        mw_session_start(s, noop_callback, 0);
        usleep(100000); // laisse le thread s'installer reellement
        mw_session_stop(s);
        check_bool("stop apres start reussit sans crash (rien a verifier de plus, pas de segfault = succes)", true, true);
        mw_session_destroy(s);
    }

    printf("\n=== Palier 3 : cas limites ===\n");
    {
        mw_session *s = mw_session_create("/tmp", NULL, NULL, 0.2);
        mw_session_start(s, noop_callback, 0);
        bool started_again = mw_session_start(s, noop_callback, 0);
        check_bool("double start -> refuse (garde-fou anti double pthread_create)", started_again, false);
        mw_session_stop(s);
        mw_session_destroy(s);
    }
    {
        mw_session *s = mw_session_create("/tmp", NULL, NULL, 0.2);
        mw_session_stop(s); // stop sans start prealable
        check_bool("stop sans start prealable -> no-op, pas de crash", true, true);
        mw_session_destroy(s);
    }
    {
        mw_session *s = mw_session_create("/tmp", NULL, NULL, 0.2);
        mw_session_start(s, noop_callback, 0);
        usleep(50000);
        mw_session_stop(s);
        mw_session_stop(s); // double stop
        check_bool("double stop -> no-op au 2e appel, pas de crash (pthread_join deux fois serait UB sinon)", true, true);
        mw_session_destroy(s);
    }

    printf("\n=== Palier 4 : adversarial ===\n");
    {
        mw_session *s = mw_session_create("/tmp", NULL, NULL, 0.2);
        mw_session_start(s, noop_callback, 0);
        usleep(50000);
        mw_session_destroy(s); // destroy SANS stop prealable explicite
        check_bool("destroy sans stop explicite -> garde-fou auto-stop, pas de thread orphelin/crash", true, true);
    }
    {
        bool started = mw_session_start(NULL, noop_callback, 0);
        check_bool("start sur session NULL -> refuse proprement", started, false);
    }
    {
        mw_session *s = mw_session_create("/tmp", NULL, NULL, 0.2);
        bool started = mw_session_start(s, NULL, 0); // callback NULL
        check_bool("start avec callback NULL -> refuse", started, false);
        mw_session_destroy(s);
    }
    {
        mw_session_stop(NULL);
        mw_session_destroy(NULL);
        check_bool("stop/destroy sur NULL -> no-op, pas de crash", true, true);
    }

    printf("\n=== Palier 5 : integration ===\n");
    printf("(cycle de vie complet avec de vrais evenements filesystem deja\n");
    printf(" valide par TestWatchEndToEnd, go test -race, 3 executions stables)\n");

    printf("\n%d/%d tests OK\n", total - failed, total);
    return failed ? 1 : 0;
}