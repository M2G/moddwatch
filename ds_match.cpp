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
          // len 0 = true
          // len >= 1 + pattern[0...3] == "*" = true
          // len > 0 + pattern[0] == "{" = true
     }


}