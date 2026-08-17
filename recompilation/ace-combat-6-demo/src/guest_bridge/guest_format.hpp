#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

// Guest printf-style formatting over guest memory.  Included from
// guest_bridge.cpp inside its anonymous namespace; it reads guest strings
// and integer arguments only through GuestMemory and never dereferences
// host pointers derived from guest values.
struct GuestFormatSpec final {
  bool left{};
  bool alternate{};
  bool zero_pad{};
  bool plus{};
  bool space{};
  int width{-1};
  int precision{-1};
  char length{};
};
[[nodiscard]] bool load_guest_string(GuestMemory &memory, std::uint32_t address,
                                     std::size_t limit, std::string *result) {
  result->clear();
  for (std::size_t index = 0; index < limit; ++index) {
    if (address > std::numeric_limits<std::uint32_t>::max() - index ||
        !memory.mapped(address + static_cast<std::uint32_t>(index), 1U)) {
      return false;
    }
    const auto value =
        memory.load_u8(address + static_cast<std::uint32_t>(index));
    if (value == 0U) {
      return true;
    }
    result->push_back(static_cast<char>(value));
  }
  return false;
}
[[nodiscard]] bool next_guest_argument(GuestMemory &memory,
                                       std::uint32_t *cursor,
                                       std::uint64_t *value) {
  if (*cursor > std::numeric_limits<std::uint32_t>::max() - 8U ||
      !memory.mapped(*cursor, 8U)) {
    return false;
  }
  *value = memory.load_u64(*cursor);
  *cursor += 8U;
  return true;
}
[[nodiscard]] std::string apply_format_width(std::string value,
                                             const GuestFormatSpec &spec,
                                             bool numeric) {
  if (spec.width < 0 || static_cast<std::size_t>(spec.width) <= value.size()) {
    return value;
  }
  const auto padding = static_cast<std::size_t>(spec.width) - value.size();
  const char fill =
      spec.zero_pad && numeric && !spec.left && spec.precision < 0 ? '0' : ' ';
  if (spec.left) {
    value.append(padding, ' ');
    return value;
  }
  if (fill == '0' && numeric && !value.empty() &&
      (value.front() == '-' || value.front() == '+' || value.front() == ' ')) {
    value.insert(1U, padding, '0');
    return value;
  }
  if (fill == '0' && numeric && value.starts_with("0x")) {
    value.insert(2U, padding, '0');
    return value;
  }
  value.insert(0U, padding, fill);
  return value;
}
[[nodiscard]] bool
format_guest_string(GuestMemory &memory, std::uint32_t buffer,
                    std::uint32_t count, std::uint32_t format,
                    std::uint32_t arguments, std::string *output) {
  std::string pattern;
  if (!load_guest_string(memory, format, 4096U, &pattern)) {
    return false;
  }
  output->clear();
  for (std::size_t index = 0; index < pattern.size();) {
    if (pattern[index] != '%') {
      output->push_back(pattern[index++]);
      continue;
    }
    ++index;
    if (index == pattern.size()) {
      return false;
    }
    if (pattern[index] == '%') {
      output->push_back('%');
      ++index;
      continue;
    }
    GuestFormatSpec spec;
    bool parsing_flags = true;
    while (parsing_flags && index < pattern.size()) {
      switch (pattern[index]) {
      case '-':
        spec.left = true;
        break;
      case '#':
        spec.alternate = true;
        break;
      case '0':
        spec.zero_pad = true;
        break;
      case '+':
        spec.plus = true;
        break;
      case ' ':
        spec.space = true;
        break;
      default:
        parsing_flags = false;
        continue;
      }
      ++index;
    }
    if (index < pattern.size() && pattern[index] == '*') {
      std::uint64_t raw{};
      if (!next_guest_argument(memory, &arguments, &raw)) {
        return false;
      }
      const auto width = static_cast<std::int32_t>(raw);
      spec.left = width < 0;
      spec.width = width < 0 ? -width : width;
      ++index;
    } else {
      spec.width = 0;
      while (index < pattern.size() && pattern[index] >= '0' &&
             pattern[index] <= '9') {
        if (spec.width > 100000) {
          return false;
        }
        spec.width = spec.width * 10 + (pattern[index] - '0');
        ++index;
      }
      if (spec.width == 0) {
        spec.width = -1;
      }
    }
    if (index < pattern.size() && pattern[index] == '.') {
      ++index;
      spec.precision = 0;
      if (index < pattern.size() && pattern[index] == '*') {
        std::uint64_t raw{};
        if (!next_guest_argument(memory, &arguments, &raw)) {
          return false;
        }
        spec.precision = static_cast<std::int32_t>(raw);
        ++index;
      } else {
        while (index < pattern.size() && pattern[index] >= '0' &&
               pattern[index] <= '9') {
          if (spec.precision > 100000) {
            return false;
          }
          spec.precision = spec.precision * 10 + (pattern[index] - '0');
          ++index;
        }
      }
    }
    if (index < pattern.size() &&
        (pattern[index] == 'h' || pattern[index] == 'l' ||
         pattern[index] == 'z')) {
      spec.length = pattern[index++];
      if (spec.length == 'l' && index < pattern.size() &&
          pattern[index] == 'l') {
        ++index;
        spec.length = 'L';
      }
    }
    if (index == pattern.size()) {
      return false;
    }
    const char conversion = pattern[index++];
    if (conversion == 's') {
      std::uint64_t raw{};
      std::string value;
      if (!next_guest_argument(memory, &arguments, &raw) ||
          !load_guest_string(memory, static_cast<std::uint32_t>(raw), 4096U,
                             &value)) {
        return false;
      }
      if (spec.precision >= 0 &&
          static_cast<std::size_t>(spec.precision) < value.size()) {
        value.resize(static_cast<std::size_t>(spec.precision));
      }
      output->append(apply_format_width(std::move(value), spec, false));
      continue;
    }
    if (conversion == 'c') {
      std::uint64_t raw{};
      if (!next_guest_argument(memory, &arguments, &raw)) {
        return false;
      }
      std::string value(1U, static_cast<char>(raw & 0xFFU));
      output->append(apply_format_width(std::move(value), spec, false));
      continue;
    }
    if (conversion != 'd' && conversion != 'i' && conversion != 'u' &&
        conversion != 'x' && conversion != 'X' && conversion != 'o' &&
        conversion != 'p') {
      return false;
    }
    std::uint64_t raw{};
    if (!next_guest_argument(memory, &arguments, &raw)) {
      return false;
    }
    const bool signed_conversion = conversion == 'd' || conversion == 'i';
    const bool pointer_conversion = conversion == 'p';
    const bool wide_integer = spec.length == 'l' || spec.length == 'L' ||
                              spec.length == 'z' || pointer_conversion;
    std::ostringstream stream;
    if (pointer_conversion) {
      stream << "0x" << std::hex << static_cast<std::uint32_t>(raw);
    } else if (signed_conversion) {
      const auto value = wide_integer ? static_cast<std::int64_t>(raw)
                                      : static_cast<std::int32_t>(raw);
      if (value >= 0 && spec.plus) {
        stream << '+';
      } else if (value >= 0 && spec.space) {
        stream << ' ';
      }
      stream << value;
    } else {
      const auto value = wide_integer ? raw : static_cast<std::uint32_t>(raw);
      if (conversion == 'x' || conversion == 'X') {
        if (spec.alternate && value != 0U) {
          stream << (conversion == 'X' ? "0X" : "0x");
        }
        if (conversion == 'X') {
          stream << std::uppercase;
        }
        stream << std::hex << value;
      } else if (conversion == 'o') {
        if (spec.alternate && value != 0U) {
          stream << '0';
        }
        stream << std::oct << value;
      } else {
        stream << value;
      }
    }
    auto value = stream.str();
    if (spec.precision > 0 && !pointer_conversion &&
        value.size() < static_cast<std::size_t>(spec.precision)) {
      std::size_t prefix = 0U;
      if (!value.empty() && (value.front() == '-' || value.front() == '+' ||
                             value.front() == ' ')) {
        prefix = 1U;
      } else if (value.starts_with("0x") || value.starts_with("0X")) {
        prefix = 2U;
      }
      value.insert(
          prefix, static_cast<std::size_t>(spec.precision) - value.size(), '0');
    }
    output->append(apply_format_width(std::move(value), spec, true));
  }
  if (count == 0U) {
    return true;
  }
  if (!memory.mapped(buffer, count)) {
    return false;
  }
  const auto writable = std::min<std::size_t>(output->size(), count - 1U);
  for (std::size_t index = 0; index < writable; ++index) {
    memory.store_u8(buffer + static_cast<std::uint32_t>(index),
                    static_cast<std::uint8_t>((*output)[index]));
  }
  memory.store_u8(buffer + static_cast<std::uint32_t>(writable), 0U);
  return true;
}
