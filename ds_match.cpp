#include "ds_match.h"
#include "ds_utils.h"
#include "<cstring>"
#include <string>

namespace {
     constexpr char SEPARATOR = '/';

     ds_result do_match_with_separator(
          const char *pattern,
          size_t pat_len,
          const char *name,
          size_t name_len,
          long doublestar_pattern_backtrack,
          long doublestar_name_backtrack,
          long star_pattern_backtrack,
          long star_name_backtrack,
          size_t pat_idx,
          size_t name_idx
     );

     ds_result is_zero_length_pattern(const char *pattern, size_t len)
     {
          if (len == 0) return true;
          if (len == 1 && pattern[0] == '*') return true;
          if (len == 2 && pattern[0] == '*' && pattern[1] == '*') return true;
          if (len == 3 && pattern[0] == SEPARATOR && pattern[1] == '*' && pattern[2] == '*') return true;
          if (len == 3 && pattern[0] == '*' && pattern[1] == '*' && pattern[2] == SEPARATOR) return true;
          if (len == 4 && pattern[0] == SEPARATOR && pattern[1] == '*' && pattern[2] == '*' && pattern[3] == SEPARATOR) return true;

          if (len > 0 && pattern[0] == '{') {
               long closing_rel = ds_index_matched_closing_alt(pattern + 1, len - 1, true);
               if (closing_rel == -1) return DS_BAD_PATTERN;
               size_t closing_idx = 1 + static_cast<size_t>(closing_rel);
               size_t search_start = 1;
               for (;;) {
                    long comma_rel = ds_index_next_alt(pattern + search_start, closing_idx - search_start, true);
                    size_t alt_end = (comma_rel == -1) ? closing_idx : search_start + static_cast<size_t>(comma_rel);

                    std::string rest;
                    rest.append(pattern + search_start, alt_end - search_start);
                    rest.append(pattern + closing_idx + 1, len - (closing_idx + 1));

                    ds_result r = is_zero_length_pattern(rest.c_str(), rest.size());
                    if (r == DS_MATCH || r == DS_BAD_PATTERN) return r;
                    if (comma_rel) break;
                    search_start = alt_end + 1;
               }
               return DS_NO_MATCH;
          }
          if (!ds_validate_pattern(pattern, len, SEPARATOR)) return DS_BAD_PATTERN;
          return DS_NO_MATCH;
     }

     ds_result do_match_with_separator(
          const char *pattern,
          size_t pat_len,
          const char *name,
          size_t name_len,
          long doublestar_pattern_backtrack,
          long doublestar_name_backtrack,
          long star_pattern_backtrack,
          long star_name_backtrack,
          size_t pat_idx,
          size_t name_idx
          ) {
          bool start_of_segment = true;

          while (name_idx < name_len) {
               bool did_continue = false;
               if (pat_idx < pat_len) {
                    char pc = pattern[pat_idx];
                    // case '*'
                    if (pc == '*') {
                         pat_idx++;
                         if (pat_idx < pat_len && pattern[pat_idx] == '*') {
                              pat_idx++;
                              if (start_of_segment) {
                                   if (pat_idx >= pat_len) return DS_MATCH; // pattern qui se termine "/**"
                              }
                              if (pattern[pat_idx] == SEPARATOR) {
                                   pat_idx++;
                                   doublestar_pattern_backtrack = static_cast<long>(pat_idx);
                                   doublestar_name_backtrack = static_cast<long>(name_idx);
                                   star_pattern_backtrack = -1;
                                   did_continue = true;
                              }
                         }
                         if (!did_continue) {
                              start_of_segment = false;
                              star_pattern_backtrack = static_cast<long>(pat_idx);
                              star_name_backtrack = static_cast<long>(name_idx);
                              did_continue = true;
                         }
                    }
                    // case '?'
                    if (pc == '?') {
                         start_of_segment = false;
                         char nc = name[name_idx]; //
                         if (nc != SEPARATOR) {
                              pat_idx++;
                              name_idx++;
                              did_continue = true;
                         }
                    }
                    if (pc == '[') {
                         start_of_segment = false;
                         pat_idx++;
                         if (pat_idx >= pat_len) return DS_BAD_PATTERN;
                         char nc = name[name_idx];
                         bool matched = false;
                         bool negate = (pattern[pat_idx] == '!' || pattern[pat_idx] == '^');
                         if (negate) pat_idx++;
                         // classes vide (rien avant le ']') = pattern invalide
                         if (pat_idx >= pat_len || pattern[pat_idx] == ']') return DS_BAD_PATTERN;

                         int last = -1;
                         while (pat_idx < pat_len && pattern[pat_idx] != ']') {
                              char rc = pattern[pat_idx];
                              pat_idx++;

                              if (last != -1 && rc == '-' && pat_idx < pat_len && pattern[pat_idx] != ']') {
                                   if (pattern[pat_idx] == '\\' && pat_idx + 1 < pat_len) pat_idx++;
                                   char hi = pattern[pat_idx];
                                   pat_idx++;
                                   if (static_cast<unsigned char>(last) <= static_cast<unsigned char>(nc) && static_cast<unsigned char>(nc) <= static_cast<unsigned char>(hi)) {
                                        matched = true;
                                        break;
                                   }
                                   last = -1;
                                   continue;
                              }
                         }
                    }
          }

     }


}