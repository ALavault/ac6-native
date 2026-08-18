#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace ac6demo {

struct GuestImage final {
  std::uint32_t base{};
  std::uint32_t entry_point{};
  std::uint32_t tls_address{};
  std::uint32_t tls_data_size{};
  std::uint32_t tls_raw_size{};
  std::uint32_t stack_size{};
  // XEX_HEADER_SYSTEM_FLAGS (key 0x00030000), the bitmask
  // XexCheckExecutablePrivilege tests bit by bit.
  std::uint32_t system_flags{};
  std::vector<std::byte> bytes;
};

// Decode the qualified XEX into its guest-addressed image. This loader accepts
// the XEX itself, not an extracted PE/basefile, and refuses unsupported XEX
// encryption or compression modes.
[[nodiscard]] GuestImage load_xex_image(const std::filesystem::path& path);

}  // namespace ac6demo
