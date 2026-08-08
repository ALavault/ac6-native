#pragma once

#include <charconv>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace ac6::detail {

inline bool parse_u32(std::string_view text, std::uint32_t& value) noexcept {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

inline bool parse_u64(std::string_view text, std::uint64_t& value) noexcept {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

inline bool parse_f32(std::string_view text, float& value) noexcept {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
         std::isfinite(value);
}

inline bool parse_bool01(std::string_view text, bool& value) noexcept {
  if (text == "0") {
    value = false;
    return true;
  }
  if (text == "1") {
    value = true;
    return true;
  }
  return false;
}

template <typename Integer>
inline bool parse_hex(std::string_view text, Integer& value) noexcept {
  if (text.size() >= 2 && text[0] == '0' &&
      (text[1] == 'x' || text[1] == 'X')) {
    text.remove_prefix(2);
  }
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);
  return !text.empty() && parsed.ec == std::errc{} &&
         parsed.ptr == text.data() + text.size();
}

inline bool parse_hex_u32(std::string_view text, std::uint32_t& value) noexcept {
  return parse_hex(text, value);
}

inline bool parse_hex_u64(std::string_view text, std::uint64_t& value) noexcept {
  return parse_hex(text, value);
}

}  // namespace ac6::detail
