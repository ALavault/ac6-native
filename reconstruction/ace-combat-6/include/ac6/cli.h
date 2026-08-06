#pragma once
#include <cstdint>
#include <string_view>
namespace ac6 {
bool parse_u32(std::string_view text, std::uint32_t& value) noexcept;
}  // namespace ac6
