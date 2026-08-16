#include "ds_utils.h"
#include "ds_match.h"
#include "mw_watch.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int total = 0, failed = 0;

// ds_utils
static void check_utils(const char *label, long got, long expected)
{
    total++;
    if (got != expected) {
        failed++;
        printf("FAIL %s : obtenu %ld, attendu %ld\n", label, got, expected);
    } else {
        printf("ok   %s\n", label);
    }
}

static void test_ds_utils(void)
{
    printf("=== ds_utils ===\n");
    check_utils("closing_alt simple",
        ds_index_matched_closing_alt("a,b,c}reste", strlen("a,b,c}reste"), true), 5);
    check_utils("closing_alt imbrique",
        ds_index_matched_closing_alt("a{b,c}d}reste", strlen("a{b,c}d}reste"), true), 7);
    check_utils("closing_alt absent",
        ds_index_matched_closing_alt("a,b,c", strlen("a,b,c"), true), -1);
    check_utils("closing_alt echappe ignore",
        ds_index_matched_closing_alt("a\\}b}reste", strlen("a\\}b}reste"), true), 4);
    check_utils("next_alt simple",
        ds_index_next_alt("a,b,c", strlen("a,b,c"), true), 1);
    check_utils("next_alt ignore virgule imbriquee",
        ds_index_next_alt("a{x,y},b", strlen("a{x,y},b"), true), 6);
    check_utils("next_alt absent",
        ds_index_next_alt("noalt", strlen("noalt"), true), -1);
    check_utils("next_alt echappee ignoree",
        ds_index_next_alt("a\\,b,c", strlen("a\\,b,c"), true), 4);
}

// ds_match
static void check_match(const char *pattern, const char *name, ds_result expected)
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

static void test_ds_match(void)
{
    printf("\n=== ds_match ===\n");
    check_match("main.c", "main.c", DS_MATCH);
    check_match("main.c", "main.h", DS_NO_MATCH);
    check_match("*.c", "main.c", DS_MATCH);
    check_match("*.c", "src/main.c", DS_NO_MATCH);
    check_match("main.?", "main.c", DS_MATCH);
    check_match("main.[ch]", "main.h", DS_MATCH);
    check_match("main.[ch]", "main.x", DS_NO_MATCH);
    check_match("main.[!a-c]", "main.d", DS_MATCH);
    check_match("src/*.c", "src/main.c", DS_MATCH);
    check_match("src/*.c", "src/sub/main.c", DS_NO_MATCH);
    check_match("**/main.c", "src/sub/main.c", DS_MATCH);
    check_match("src/**", "src", DS_MATCH);
    check_match("**/.git/**", "src/.git/config", DS_MATCH);
    check_match("**.tmp", "foo.tmp", DS_MATCH);
    check_match("**.tmp", "src/foo.tmp", DS_NO_MATCH);
    check_match("main.{c,h}", "main.c", DS_MATCH);
    check_match("main.{c,h}", "main.x", DS_NO_MATCH);
    check_match("some{thing,}", "some", DS_MATCH);
    check_match("main.[c", "main.[c", DS_BAD_PATTERN);
    check_match("main.[]", "main.[]", DS_BAD_PATTERN);
    check_match("main.[!]", "main.[!]", DS_BAD_PATTERN);
    check_match("[a-c]", "b", DS_MATCH);
    check_match("[a-c]", "d", DS_NO_MATCH);
    check_match("[]a]", "]", DS_BAD_PATTERN);
    check_match("main\\*.c", "main*.c", DS_MATCH);
    check_match("main\\*.c", "mainX.c", DS_NO_MATCH);
    check_match("a\\-z", "a-z", DS_MATCH);
    check_match("[a\\]b]", "]", DS_MATCH);
    check_match("abc[", "abc", DS_BAD_PATTERN);
    check_match("abc{", "abc", DS_BAD_PATTERN);
    check_match("abc}", "abc", DS_BAD_PATTERN);
    check_match("abcdef", "abc", DS_NO_MATCH);
}

// mw_watch
static void check_bool(const char *label, bool got, bool expected)
{
    total++;
    if (got != expected) {
        failed++;
        printf("FAIL %s : obtenu %d, attendu %d\n", label, got, expected);
    } else {
        printf("ok   %s\n", label);
    }
}

static void noop_cb(const char *path, bool c, bool u, bool r, bool rn, uintptr_t ud)
{
    (void)path; (void)c; (void)u; (void)r; (void)rn; (void)ud;
}

static void test_mw_watch(void)
{
    printf("\n=== mw_watch (vraie libfswatch) ===\n");

    {
        mw_session *s = mw_session_create("/tmp", 0.2);
        check_bool("session creee avec succes", s != NULL, true);
        mw_session_destroy(s);
    }
    {
        mw_session *s = mw_session_create("/tmp", 1.0);
        check_bool("session creee avec latence differente", s != NULL, true);
        mw_session_destroy(s);
    }
    {
        mw_session *s = mw_session_create("/tmp", 0.2);
        bool started = mw_session_start(s, noop_cb, 0);
        check_bool("start reussit", started, true);
        mw_session_stop(s);
        mw_session_destroy(s);
    }
    {
        mw_session *s = mw_session_create("/tmp", 0.2);
        mw_session_start(s, noop_cb, 0);
        bool second = mw_session_start(s, noop_cb, 0);
        check_bool("double start refuse", second, false);
        mw_session_stop(s);
        mw_session_destroy(s);
    }
    {
        mw_session *s = mw_session_create("/tmp", 0.2);
        mw_session_stop(s);
        check_bool("stop sans start ne plante pas", true, true);
        mw_session_destroy(s);
    }
    {
        mw_session *s = mw_session_create("/tmp", 0.2);
        mw_session_start(s, noop_cb, 0);
        mw_session_destroy(s);
        check_bool("destroy sans stop ne plante pas", true, true);
    }
    {
        mw_session_stop(NULL);
        mw_session_destroy(NULL);
        bool started = mw_session_start(NULL, noop_cb, 0);
        check_bool("start sur session NULL refuse", started, false);
    }
}

int main(void)
{
    test_ds_utils();
    test_ds_match();
    test_mw_watch();

    printf("\n%d/%d tests OK\n", total - failed, total);
    return failed ? 1 : 0;
}