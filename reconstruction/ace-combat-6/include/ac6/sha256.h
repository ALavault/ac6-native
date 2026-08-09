#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace ac6 {

using Sha256Digest = std::array<std::uint8_t, 32>;

Sha256Digest sha256_bytes(std::span<const std::uint8_t> bytes) noexcept;
bool sha256_file(const std::filesystem::path& path, Sha256Digest& digest,
                 std::uint64_t maximum_size = UINT64_MAX) noexcept;
std::string sha256_hex(const Sha256Digest& digest);
bool parse_sha256(std::string_view text, Sha256Digest& digest) noexcept;
bool equal_sha256(std::string_view text, const Sha256Digest& digest) noexcept;

}  // namespace ac6
