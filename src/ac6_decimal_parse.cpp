#include "ac6_decimal_parse.h"

#include <limits>

namespace ac6::decimal_parse {
namespace {

bool IsAsciiSpace(char value) {
  return value == ' ' || (value >= '\t' && value <= '\r');
}

}  // namespace

int32_t ParseAtoi(std::string_view input) {
  size_t index = 0;
  while (index < input.size() && IsAsciiSpace(input[index])) ++index;

  bool negative = false;
  if (index < input.size() && (input[index] == '+' || input[index] == '-')) {
    negative = input[index] == '-';
    ++index;
  }

  constexpr uint64_t kPositiveLimit =
      static_cast<uint64_t>(std::numeric_limits<int32_t>::max());
  constexpr uint64_t kNegativeLimit = kPositiveLimit + 1u;
  const uint64_t limit = negative ? kNegativeLimit : kPositiveLimit;
  uint64_t value = 0;
  for (; index < input.size(); ++index) {
    const unsigned char current = static_cast<unsigned char>(input[index]);
    if (current < '0' || current > '9') break;
    const uint32_t digit = current - '0';
    if (value > (limit - digit) / 10u) {
      value = limit;
      while (++index < input.size() && input[index] >= '0' &&
             input[index] <= '9') {
      }
      break;
    }
    value = value * 10u + digit;
  }

  if (negative) {
    if (value == kNegativeLimit) return std::numeric_limits<int32_t>::min();
    return -static_cast<int32_t>(value);
  }
  return static_cast<int32_t>(value);
}

}  // namespace ac6::decimal_parse
