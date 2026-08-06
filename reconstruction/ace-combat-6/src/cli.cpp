#include "ac6/cli.h"
#include <charconv>
namespace ac6 {
bool parse_u32(std::string_view text, std::uint32_t& value) noexcept {
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}
}  // namespace ac6
