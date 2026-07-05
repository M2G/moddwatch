#ifndef GLOBMATCH_H
#define GLOBMATCH_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Teste si "path" (chemin normalisé, séparateur '/') correspond au pattern "pattern".
 *
 * Retourne true si match, false sinon.
 * Ne fait aucune allocation heap (matcher récursif sur buffers statiques).
 */
bool glob_match(const char *pattern, const char *path);

/*
 * Teste "path" contre une liste de patterns d'inclusion et une liste
 * de patterns d'exclusion (équivalent MatchAny + logique include/exclude
 * de moddwatch : exclude prioritaire sur include).
 *
 * includes/excludes: tableaux de chaînes C, terminés par NULL.
 */
bool glob_match_any(const char **patterns, const char *path);

bool glob_should_include(const char **includes, const char **excludes,
                          const char *path);

#endif