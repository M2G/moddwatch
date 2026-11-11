/* Standalone check of the length-based C API (ds_match_n).
 *
 *   cc -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -Ifilter \
 *      filter/ds_match.c filter/ds_utils.c filter/ctest/ds_match_asan.c -o /tmp/ds_asan
 *   /tmp/ds_asan [iterations]
 *
 * Each pattern and name is copied into a heap buffer of EXACTLY its length: no
 * NUL terminator and nothing after it, so AddressSanitizer reports any read
 * past the end. ds_match_n() must also give the same answer as ds_match() on
 * the same input, which proves the refactoring did not change the engine.
 */
#include "ds_match.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t rng_state = 88172645463325252ULL;

static uint64_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}

/* The pattern alphabet used by the Go stress test; "\xc3\xa9" is a UTF-8 'e acute'. */
static const char PAT_ALPHA[] = "ab/*?[]{},\\^!-.\xc3\xa9";
static const char NAME_ALPHA[] = "ab/.\xc3\xa9";

static char *exact_copy(const char *src, size_t len) {
    static char dummy[1];
    char *p = malloc(len); /* under ASan, malloc(0) still gives a zero-size region */
    if (!p) return dummy;
    memcpy(p, src, len);
    return p;
}

static void release(char *p) {
    static char *unused;
    (void)unused;
    free(p);
}

static size_t gen(char *dst, const char *alpha, size_t alpha_len, size_t max_len) {
    size_t len = (size_t)(rng() % (max_len + 1));
    for (size_t i = 0; i < len; i++) dst[i] = alpha[rng() % alpha_len];
    return len;
}

static int check(const char *pat, size_t pat_len, const char *name, size_t name_len,
                 unsigned long *matches, unsigned long *bad) {
    char *p = exact_copy(pat, pat_len);
    char *n = exact_copy(name, name_len);
    ds_result got = ds_match_n(p, pat_len, n, name_len);
    release(p);
    release(n);

    /* NUL-terminated copies for the strlen-based entry point */
    char *pz = malloc(pat_len + 1);
    char *nz = malloc(name_len + 1);
    memcpy(pz, pat, pat_len);
    pz[pat_len] = '\0';
    memcpy(nz, name, name_len);
    nz[name_len] = '\0';
    ds_result want = ds_match(pz, nz);
    free(pz);
    free(nz);

    if (got != want) {
        fprintf(stderr, "MISMATCH pattern \"%.*s\" name \"%.*s\": ds_match_n=%d ds_match=%d\n",
                (int)pat_len, pat, (int)name_len, name, got, want);
        return 1;
    }
    if (got == DS_MATCH) (*matches)++;
    if (got == DS_BAD_PATTERN) (*bad)++;
    return 0;
}

int main(int argc, char **argv) {
    unsigned long n = argc > 1 ? strtoul(argv[1], NULL, 10) : 2000000UL;
    unsigned long matches = 0, bad = 0, failures = 0;

    /* hand-picked edge cases first */
    static const char *edge[][2] = {
        {"", ""}, {"*", ""}, {"**", ""}, {"/**", ""}, {"**/", ""}, {"/**/", ""},
        {"a", ""}, {"", "a"}, {"[", "a"}, {"[a", "a"}, {"a[", "b"}, {"\\", "a"}, {"a\\", "a"},
        {"{", ""}, {"{a", "a"}, {"{a,", "a"}, {"{a,b", "b"}, {"{}", ""}, {"{,}", ""},
        {"[^", "a"}, {"[^]", "a"}, {"[]", "a"}, {"[a-", "a"}, {"[a-]", "-"}, {"{[}0]", "0"},
        {"**/a", "a"}, {"a/**", "a/"}, {"a/**/b", "a/b"}, {"a**/b", "ax/b"},
    };
    for (size_t i = 0; i < sizeof edge / sizeof edge[0]; i++)
        failures += check(edge[i][0], strlen(edge[i][0]), edge[i][1], strlen(edge[i][1]), &matches, &bad);

    char pat[64], name[64];
    for (unsigned long i = 0; i < n; i++) {
        size_t pl = gen(pat, PAT_ALPHA, sizeof PAT_ALPHA - 1, 14);
        size_t nl = gen(name, NAME_ALPHA, sizeof NAME_ALPHA - 1, 10);
        failures += check(pat, pl, name, nl, &matches, &bad);
    }

    printf("%s: %lu random + %zu edge cases, %lu matches, %lu bad patterns, %lu mismatches\n",
           failures ? "FAIL" : "ok", n, sizeof edge / sizeof edge[0], matches, bad, failures);
    return failures ? 1 : 0;
}