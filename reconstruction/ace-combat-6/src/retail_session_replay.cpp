#include "ac6/retail_session_replay.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>

namespace ac6::retail {
namespace {

constexpr char kMagic[] = "AC6RTPLY\0";
constexpr std::size_t kMagicSize = sizeof(kMagic) - 1u;
constexpr std::uint32_t kMaximumMission = 15u;

void write_u16(std::ostream& output, std::uint16_t value) {
  const std::array<unsigned char, 2> bytes{
      static_cast<unsigned char>(value & 0xffu),
      static_cast<unsigned char>((value >> 8u) & 0xffu)};
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

void write_u32(std::ostream& output, std::uint32_t value) {
  const std::array<unsigned char, 4> bytes{
      static_cast<unsigned char>(value & 0xffu),
      static_cast<unsigned char>((value >> 8u) & 0xffu),
      static_cast<unsigned char>((value >> 16u) & 0xffu),
      static_cast<unsigned char>((value >> 24u) & 0xffu)};
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
}

bool read_exact(std::istream& input, void* destination, std::size_t size) {
  input.read(static_cast<char*>(destination), static_cast<std::streamsize>(size));
  return static_cast<bool>(input);
}

bool read_u16(std::istream& input, std::uint16_t& value) {
  std::array<unsigned char, 2> bytes{};
  if (!read_exact(input, bytes.data(), bytes.size())) return false;
  value = static_cast<std::uint16_t>(bytes[0]) |
          (static_cast<std::uint16_t>(bytes[1]) << 8u);
  return true;
}

bool read_u32(std::istream& input, std::uint32_t& value) {
  std::array<unsigned char, 4> bytes{};
  if (!read_exact(input, bytes.data(), bytes.size())) return false;
  value = static_cast<std::uint32_t>(bytes[0]) |
          (static_cast<std::uint32_t>(bytes[1]) << 8u) |
          (static_cast<std::uint32_t>(bytes[2]) << 16u) |
          (static_cast<std::uint32_t>(bytes[3]) << 24u);
  return true;
}

bool nonzero_digest(const Sha256Digest& digest) noexcept {
  return std::any_of(digest.begin(), digest.end(), [](std::uint8_t byte) {
    return byte != 0;
  });
}

}  // namespace

bool RetailSessionReplay::valid() const noexcept {
  return version == kCurrentVersion && mission_id != 0 &&
      mission_id <= kMaximumMission && loadout.valid() &&
      nonzero_digest(content_index_sha256) && !frames.empty() &&
      frames.size() <= kMaximumFrames;
}

bool RetailSessionReplay::write_file(const std::filesystem::path& path) const {
  if (path.empty() || !valid()) return false;
  std::filesystem::path temporary = path;
  temporary += ".tmp";
  std::error_code error;
  std::filesystem::remove(temporary, error);
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output.write(kMagic, static_cast<std::streamsize>(kMagicSize));
  write_u32(output, version);
  write_u32(output, mission_id);
  write_u32(output, loadout.aircraft_id);
  write_u32(output, loadout.weapon_id);
  write_u32(output, loadout.capability_data_valid ? 1u : 0u);
  output.write(reinterpret_cast<const char*>(content_index_sha256.data()),
               static_cast<std::streamsize>(content_index_sha256.size()));
  write_u32(output, static_cast<std::uint32_t>(frames.size()));
  for (const InputFrame frame : frames) {
    write_u16(output, static_cast<std::uint16_t>(frame.pitch));
    write_u16(output, static_cast<std::uint16_t>(frame.roll));
    write_u16(output, static_cast<std::uint16_t>(frame.yaw));
    output.put(static_cast<char>(frame.throttle));
    write_u16(output, frame.buttons);
  }
  output.flush();
  const bool written = static_cast<bool>(output);
  output.close();
  if (!written) {
    std::filesystem::remove(temporary, error);
    return false;
  }
  std::filesystem::rename(temporary, path, error);
  if (error) {
    std::filesystem::remove(temporary, error);
    return false;
  }
  return true;
}

bool RetailSessionReplay::read_file(const std::filesystem::path& path) {
  if (path.empty()) return false;
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  std::array<char, kMagicSize> magic{};
  if (!read_exact(input, magic.data(), magic.size()) ||
      std::memcmp(magic.data(), kMagic, kMagicSize) != 0) return false;

  RetailSessionReplay parsed;
  std::uint32_t capability = 0;
  std::uint32_t count = 0;
  if (!read_u32(input, parsed.version) || !read_u32(input, parsed.mission_id) ||
      !read_u32(input, parsed.loadout.aircraft_id) ||
      !read_u32(input, parsed.loadout.weapon_id) || !read_u32(input, capability) ||
      !read_exact(input, parsed.content_index_sha256.data(),
                  parsed.content_index_sha256.size()) ||
      !read_u32(input, count) || count > kMaximumFrames) {
    return false;
  }
  parsed.loadout.capability_data_valid = capability != 0;
  parsed.frames.reserve(count);
  for (std::uint32_t index = 0; index < count; ++index) {
    InputFrame frame{};
    std::uint16_t raw_pitch = 0;
    std::uint16_t raw_roll = 0;
    std::uint16_t raw_yaw = 0;
    if (!read_u16(input, raw_pitch) || !read_u16(input, raw_roll) ||
        !read_u16(input, raw_yaw) || !read_exact(input, &frame.throttle, 1) ||
        !read_u16(input, frame.buttons)) {
      return false;
    }
    std::memcpy(&frame.pitch, &raw_pitch, sizeof(frame.pitch));
    std::memcpy(&frame.roll, &raw_roll, sizeof(frame.roll));
    std::memcpy(&frame.yaw, &raw_yaw, sizeof(frame.yaw));
    parsed.frames.push_back(frame);
  }
  char extra = 0;
  if (input.read(&extra, 1)) return false;
  if (!input.eof() || !parsed.valid()) return false;
  *this = std::move(parsed);
  return true;
}

}  // namespace ac6::retail
