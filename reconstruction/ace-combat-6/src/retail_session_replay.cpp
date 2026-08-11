#include "ac6/retail_session_replay.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <vector>

namespace ac6::retail {
namespace {

constexpr char kMagic[] = "AC6RTPLY\0";
constexpr std::size_t kMagicSize = sizeof(kMagic) - 1u;
constexpr std::uint32_t kMaximumMission = 15u;
constexpr std::uint64_t kLegacySeed = 0xAC60000000000001ull;

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

void write_u64(std::ostream& output, std::uint64_t value) {
  const std::array<unsigned char, 8> bytes{
      static_cast<unsigned char>(value & 0xffu),
      static_cast<unsigned char>((value >> 8u) & 0xffu),
      static_cast<unsigned char>((value >> 16u) & 0xffu),
      static_cast<unsigned char>((value >> 24u) & 0xffu),
      static_cast<unsigned char>((value >> 32u) & 0xffu),
      static_cast<unsigned char>((value >> 40u) & 0xffu),
      static_cast<unsigned char>((value >> 48u) & 0xffu),
      static_cast<unsigned char>((value >> 56u) & 0xffu)};
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

bool read_u64(std::istream& input, std::uint64_t& value) {
  std::array<unsigned char, 8> bytes{};
  if (!read_exact(input, bytes.data(), bytes.size())) return false;
  value = static_cast<std::uint64_t>(bytes[0]) |
          (static_cast<std::uint64_t>(bytes[1]) << 8u) |
          (static_cast<std::uint64_t>(bytes[2]) << 16u) |
          (static_cast<std::uint64_t>(bytes[3]) << 24u) |
          (static_cast<std::uint64_t>(bytes[4]) << 32u) |
          (static_cast<std::uint64_t>(bytes[5]) << 40u) |
          (static_cast<std::uint64_t>(bytes[6]) << 48u) |
          (static_cast<std::uint64_t>(bytes[7]) << 56u);
  return true;
}

bool nonzero_digest(const Sha256Digest& digest) noexcept {
  return std::any_of(digest.begin(), digest.end(), [](std::uint8_t byte) {
    return byte != 0;
  });
}

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
}

void append_frame(std::vector<std::uint8_t>& bytes, InputFrame frame) {
  append_u16(bytes, static_cast<std::uint16_t>(frame.pitch));
  append_u16(bytes, static_cast<std::uint16_t>(frame.roll));
  append_u16(bytes, static_cast<std::uint16_t>(frame.yaw));
  bytes.push_back(frame.throttle);
  append_u16(bytes, frame.buttons);
}

}  // namespace

Sha256Digest RetailSessionReplay::input_digest(std::size_t frame_count) const noexcept {
  if (frame_count > frames.size()) return {};
  std::vector<std::uint8_t> bytes;
  bytes.reserve(frame_count * 9u);
  for (std::size_t index = 0; index < frame_count; ++index) {
    append_frame(bytes, frames[index]);
  }
  return sha256_bytes(bytes);
}

Sha256Digest RetailSessionReplay::input_digest() const noexcept {
  return input_digest(frames.size());
}

bool RetailSessionReplay::valid() const noexcept {
  if (version != kCurrentVersion || mission_id == 0 ||
      mission_id > kMaximumMission ||
      static_cast<std::uint8_t>(difficulty) >
          static_cast<std::uint8_t>(RetailDifficulty::Ace) ||
      !loadout.valid() || !nonzero_digest(content_index_sha256) ||
      frames.empty() || frames.size() > kMaximumFrames || random_seed == 0 ||
      final_tick != frames.size() || !nonzero_digest(final_digest) ||
      final_digest != input_digest() ||
      checkpoints.size() > kMaximumCheckpoints) {
    return false;
  }
  std::uint32_t previous_frame = 0;
  for (const Checkpoint& checkpoint : checkpoints) {
    if (checkpoint.frame_index == 0 ||
        checkpoint.frame_index <= previous_frame ||
        checkpoint.frame_index > frames.size() ||
        !nonzero_digest(checkpoint.input_digest) ||
        checkpoint.input_digest != input_digest(checkpoint.frame_index)) {
      return false;
    }
    previous_frame = checkpoint.frame_index;
  }
  return true;
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
  write_u32(output, static_cast<std::uint32_t>(difficulty));
  write_u32(output, loadout.aircraft_id);
  write_u32(output, loadout.weapon_id);
  write_u32(output, loadout.capability_data_valid ? 1u : 0u);
  output.write(reinterpret_cast<const char*>(content_index_sha256.data()),
               static_cast<std::streamsize>(content_index_sha256.size()));
  write_u64(output, random_seed);
  write_u32(output, static_cast<std::uint32_t>(checkpoints.size()));
  for (const Checkpoint& checkpoint : checkpoints) {
    write_u32(output, checkpoint.frame_index);
    output.write(reinterpret_cast<const char*>(checkpoint.input_digest.data()),
                 static_cast<std::streamsize>(checkpoint.input_digest.size()));
  }
  write_u64(output, final_tick);
  output.write(reinterpret_cast<const char*>(final_digest.data()),
               static_cast<std::streamsize>(final_digest.size()));
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
  std::uint32_t file_version = 0;
  std::uint32_t capability = 0;
  std::uint32_t raw_difficulty = 0;
  std::uint32_t checkpoint_count = 0;
  std::uint32_t count = 0;
  if (!read_u32(input, file_version) || !read_u32(input, parsed.mission_id) ||
      (file_version != 1 && file_version != 2 && file_version != kCurrentVersion)) {
    return false;
  }
  if (file_version >= 2 &&
      (!read_u32(input, raw_difficulty) ||
       raw_difficulty > static_cast<std::uint32_t>(RetailDifficulty::Ace))) {
    return false;
  }
  if (file_version >= 2) {
    parsed.difficulty = static_cast<RetailDifficulty>(raw_difficulty);
  }
  if (!read_u32(input, parsed.loadout.aircraft_id) ||
      !read_u32(input, parsed.loadout.weapon_id) || !read_u32(input, capability) ||
      !read_exact(input, parsed.content_index_sha256.data(),
                  parsed.content_index_sha256.size())) {
    return false;
  }
  if (file_version == kCurrentVersion) {
    if (!read_u64(input, parsed.random_seed) ||
        !read_u32(input, checkpoint_count) ||
        checkpoint_count > kMaximumCheckpoints) {
      return false;
    }
    parsed.checkpoints.reserve(checkpoint_count);
    for (std::uint32_t index = 0; index < checkpoint_count; ++index) {
      Checkpoint checkpoint{};
      if (!read_u32(input, checkpoint.frame_index) ||
          !read_exact(input, checkpoint.input_digest.data(),
                      checkpoint.input_digest.size())) {
        return false;
      }
      parsed.checkpoints.push_back(checkpoint);
    }
    if (!read_u64(input, parsed.final_tick) ||
        !read_exact(input, parsed.final_digest.data(), parsed.final_digest.size())) {
      return false;
    }
  }
  if (!read_u32(input, count) || count > kMaximumFrames) return false;
  parsed.version = kCurrentVersion;
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
  if (file_version != kCurrentVersion) {
    // v1/v2 did not carry the deterministic metadata.  They are accepted as
    // legacy recordings and upgraded in memory with a stable migration seed;
    // the input stream itself is the reproducible final digest basis.
    parsed.random_seed = kLegacySeed;
    parsed.final_tick = parsed.frames.size();
    parsed.final_digest = parsed.input_digest();
    parsed.checkpoints.clear();
  }
  char extra = 0;
  if (input.read(&extra, 1)) return false;
  if (!input.eof() || !parsed.valid()) return false;
  *this = std::move(parsed);
  return true;
}

}  // namespace ac6::retail
