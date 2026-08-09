#include "ds_utils.h"
#include <stdio.h>
#include <string.h>

static int total = 0, failed = 0;

static void check(const char *label, const char *s, char c, bool esc, long expected)
{
    total++;
    long got = ds_index_unescaped_byte(s, strlen(s), c, esc);
    if (got != expected) {
        failed++;
        printf("FAIL [%s]: ds_index_unescaped_byte(\"%s\", '%c', %d) = %ld, attendu %ld\n",
               label, s, c, esc, got, expected);
    } else {
        printf("ok   [%s]: -> %ld\n", label, got);
    }
}

int main(void)
{
    printf("=== Palier 1 : trivial ===\n");
    check("chaine vide", "", 'x', true, -1);

    printf("\n=== Palier 2 : nominal ===\n");
    check("absent", "abc", 'x', true, -1);
    check("present au milieu", "abxcd", 'x', true, 2);
    check("present plusieurs fois -> premiere occurrence", "axbxc", 'x', true, 1);

    printf("\n=== Palier 3 : cas limites ===\n");
    check("cible en position 0", "xabc", 'x', true, 0);
    check("cible en derniere position", "abcx", 'x', true, 3);
    check("chaine d'un seul caractere, qui matche", "x", 'x', true, 0);
    check("chaine d'un seul caractere, qui ne matche pas", "y", 'x', true, -1);

    printf("\n=== Palier 4 : adversarial (echappement) ===\n");
    check("cible echappee -> ignoree, trouve la suivante", "a\\xbxc", 'x', true, 4);
    check("cible echappee, aucune autre occurrence -> absent", "a\\x", 'x', true, -1);
    check("backslash en toute derniere position -> pas de crash, pas de match", "ab\\", 'x', true, -1);
    check("double backslash = 1 backslash echappe, la cible suivante reste normale et matche", "a\\\\x", 'x', true, 3);
    check("allow_escaping=false -> le backslash ne protege plus rien", "a\\xbxc", 'x', false, 2);

    printf("\n=== Palier 5 : integration ===\n");
    printf("(aucun site d'appel actuel dans ds_match.cpp/filter.cpp - fonction\n");
    printf(" verbatim gardee pour fidelite, testee isolement uniquement)\n");

    printf("\n%d/%d tests OK\n", total - failed, total);
    return failed ? 1 : 0;
}