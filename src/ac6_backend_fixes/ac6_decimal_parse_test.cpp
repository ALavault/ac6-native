#include "../ac6_decimal_parse.h"

#include <cassert>
#include <cstdint>
#include <limits>

int main() {
  using ac6::decimal_parse::ParseAtoi;

  assert(ParseAtoi("200") == 200);
  assert(ParseAtoi("500_briefing_Main") == 500);
  assert(ParseAtoi("  \t-42x") == -42);
  assert(ParseAtoi("+17") == 17);
  assert(ParseAtoi("x17") == 0);
  assert(ParseAtoi("") == 0);
  assert(ParseAtoi("2147483648") == std::numeric_limits<int32_t>::max());
  assert(ParseAtoi("-2147483649") == std::numeric_limits<int32_t>::min());
  return 0;
}
