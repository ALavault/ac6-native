#pragma once

#include <cstdint>
#include <string_view>

namespace ac6::decimal_parse {

// Deterministic signed decimal conversion used by the PAL atoi thunk.
// Leading ASCII whitespace and one sign are accepted; parsing stops at the
// first non-digit and overflow saturates to the signed 32-bit range.
int32_t ParseAtoi(std::string_view input);

}  // namespace ac6::decimal_parse
