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

     bool is_zero_length_pattern(const char *pattern, size_t pat_len)
     {
          if (len == 0) return true;
          if (len == 1 && pattern[0] == '*') return true;
          if (len == 2 && pattern[0] == '*' && pattern[1] == '*') return true;
          if (len == 3 && pattern[0] == SEPARATOR && pattern[1] == '*' && pattern[2] == '*') return true;
          if (len == 3 && pattern[0] == '*' && pattern[1] == '*' && pattern[2] == SEPARATOR) return true;
          if (len == 4 && pattern[0] == SEPARATOR && pattern[1] == '*' && pattern[2] == '*' && pattern[3] == SEPARATOR) return true;

          if (len > 0 && pattern[0] == '{') {
               // ...
          }


         return false;
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
               //...
               if (pat_idx < pat_len) {
                    char pc = pattern[pat_idx];
                    if (pc == '*') {
                         // ...
                    }
               }
          }

     }


}