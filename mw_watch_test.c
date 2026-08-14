#include "mw_watch.h"
#include <stdio.h>
#include <unistd.h>

static int total = 0, failed = 0;

static void check(const char *label, bool got, bool expected)
{
    total++;
    if (got != expected) {
        failed++;
        printf("FAIL %s : obtenu %d, attendu %d\n", label, got, expected);
    } else {
        printf("ok   %s\n", label);
    }
}

static void noop(const char *, bool, bool, bool, bool, uintptr_t) {}

int main()
{
    // creation basique
    {
        mw_session *s = mw_session_create("/tmp", NULL, NULL, 0.2);
        check("session creee avec succes", s != NULL, true);
        mw_session_destroy(s);
    }

    // avec includes/excludes
    {
        const char *includes[] = {"*.go", "*.c", NULL};
        const char *excludes[] = {"**/.git/**", NULL};
        mw_session *s = mw_session_create("/tmp", includes, excludes, 1.0);
        check("session creee avec patterns", s != NULL, true);
        mw_session_destroy(s);
    }

    // start puis stop, cas normal
    {
        mw_session *s = mw_session_create("/tmp", NULL, NULL, 0.2);
        bool started = mw_session_start(s, noop, 0);
        check("start reussit", started, true);
        mw_session_stop(s);
        mw_session_destroy(s);
    }

    // double start refuse
    {
        mw_session *s = mw_session_create("/tmp", NULL, NULL, 0.2);
        mw_session_start(s, noop, 0);
        bool second = mw_session_start(s, noop, 0);
        check("double start refuse", second, false);
        mw_session_stop(s);
        mw_session_destroy(s);
    }

    // stop sans start ne plante pas
    {
        mw_session *s = mw_session_create("/tmp", NULL, NULL, 0.2);
        mw_session_stop(s);
        check("stop sans start ne plante pas", true, true);
        mw_session_destroy(s);
    }

    // destroy sans stop explicite (garde-fou auto-stop)
    {
        mw_session *s = mw_session_create("/tmp", NULL, NULL, 0.2);
        mw_session_start(s, noop, 0);
        mw_session_destroy(s);
        check("destroy sans stop ne plante pas", true, true);
    }

    // NULL partout, aucun crash
    {
        mw_session_stop(NULL);
        mw_session_destroy(NULL);
        bool started = mw_session_start(NULL, noop, 0);
        check("start sur session NULL refuse", started, false);
    }

    printf("\n%d/%d tests OK\n", total - failed, total);
    return failed ? 1 : 0;
}