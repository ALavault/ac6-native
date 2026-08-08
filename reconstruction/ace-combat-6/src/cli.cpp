#include "ac6/cli.h"
#include "text_parse.h"

namespace ac6 {
bool parse_u32(std::string_view text, std::uint32_t& value) noexcept {
  return detail::parse_u32(text, value);
}
}  // namespace ac6
