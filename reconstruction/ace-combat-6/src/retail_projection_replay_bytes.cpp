#include "retail_projection_replay_bytes.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace ac6::retail::detail {
namespace {

constexpr std::array<std::uint8_t, 9> kReplayMagic{'A', 'C', '6', 'R', 'T',
                                                   'P', 'L', 'Y', 0};

class ReplayBytesReader final {
public:
  explicit ReplayBytesReader(std::span<const std::uint8_t> bytes)
      : bytes_(bytes) {}

  bool read_u16(std::uint16_t &value) {
    if (remaining() < 2u)
      return false;
    value = static_cast<std::uint16_t>(bytes_[cursor_]) |
            (static_cast<std::uint16_t>(bytes_[cursor_ + 1u]) << 8u);
    cursor_ += 2u;
    return true;
  }

  bool read_u32(std::uint32_t &value) {
    if (remaining() < 4u)
      return false;
    value = static_cast<std::uint32_t>(bytes_[cursor_]) |
            (static_cast<std::uint32_t>(bytes_[cursor_ + 1u]) << 8u) |
            (static_cast<std::uint32_t>(bytes_[cursor_ + 2u]) << 16u) |
            (static_cast<std::uint32_t>(bytes_[cursor_ + 3u]) << 24u);
    cursor_ += 4u;
    return true;
  }

  bool read_u64(std::uint64_t &value) {
    if (remaining() < 8u)
      return false;
    value = 0;
    for (std::size_t index = 0; index < 8u; ++index)
      value |= static_cast<std::uint64_t>(bytes_[cursor_ + index])
               << (index * 8u);
    cursor_ += 8u;
    return true;
  }

  bool read_bytes(std::span<std::uint8_t> output) {
    if (remaining() < output.size())
      return false;
    std::copy_n(bytes_.begin() + static_cast<std::ptrdiff_t>(cursor_),
                output.size(), output.begin());
    cursor_ += output.size();
    return true;
  }

  bool consume(std::span<const std::uint8_t> expected) {
    if (remaining() < expected.size() ||
        !std::equal(expected.begin(), expected.end(),
                    bytes_.begin() + static_cast<std::ptrdiff_t>(cursor_))) {
      return false;
    }
    cursor_ += expected.size();
    return true;
  }

  bool finished() const noexcept { return cursor_ == bytes_.size(); }

private:
  std::size_t remaining() const noexcept { return bytes_.size() - cursor_; }

  std::span<const std::uint8_t> bytes_;
  std::size_t cursor_{};
};

} // namespace

bool replay_v3_matches(std::span<const std::uint8_t> bytes,
                       const RetailSessionReplay &replay) {
  ReplayBytesReader reader(bytes);
  std::uint32_t version = 0;
  std::uint32_t mission = 0;
  std::uint32_t difficulty = 0;
  std::uint32_t aircraft = 0;
  std::uint32_t weapon = 0;
  std::uint32_t capability = 0;
  Sha256Digest cache{};
  std::uint64_t seed = 0;
  std::uint32_t checkpoint_count = 0;
  if (!reader.consume(kReplayMagic) || !reader.read_u32(version) ||
      !reader.read_u32(mission) || !reader.read_u32(difficulty) ||
      !reader.read_u32(aircraft) || !reader.read_u32(weapon) ||
      !reader.read_u32(capability) || capability > 1u ||
      !reader.read_bytes(cache) || !reader.read_u64(seed) ||
      !reader.read_u32(checkpoint_count) ||
      version != RetailSessionReplay::kCurrentVersion ||
      version != replay.version || mission != replay.mission_id ||
      difficulty != static_cast<std::uint8_t>(replay.difficulty) ||
      aircraft != replay.loadout.aircraft_id ||
      weapon != replay.loadout.weapon_id ||
      (capability != 0u) != replay.loadout.capability_data_valid ||
      cache != replay.content_index_sha256 || seed != replay.random_seed ||
      checkpoint_count != replay.checkpoints.size()) {
    return false;
  }
  for (const RetailSessionReplay::Checkpoint &expected : replay.checkpoints) {
    std::uint32_t frame_index = 0;
    Sha256Digest digest{};
    if (!reader.read_u32(frame_index) || !reader.read_bytes(digest) ||
        frame_index != expected.frame_index ||
        digest != expected.input_digest) {
      return false;
    }
  }
  std::uint64_t final_tick = 0;
  Sha256Digest final_digest{};
  std::uint32_t frame_count = 0;
  if (!reader.read_u64(final_tick) || !reader.read_bytes(final_digest) ||
      !reader.read_u32(frame_count) || final_tick != replay.final_tick ||
      final_digest != replay.final_digest ||
      frame_count != replay.frames.size()) {
    return false;
  }
  for (const InputFrame expected : replay.frames) {
    std::uint16_t pitch = 0;
    std::uint16_t roll = 0;
    std::uint16_t yaw = 0;
    std::uint8_t throttle = 0;
    std::uint16_t buttons = 0;
    if (!reader.read_u16(pitch) || !reader.read_u16(roll) ||
        !reader.read_u16(yaw) ||
        !reader.read_bytes(std::span<std::uint8_t>(&throttle, 1u)) ||
        !reader.read_u16(buttons) ||
        pitch != static_cast<std::uint16_t>(expected.pitch) ||
        roll != static_cast<std::uint16_t>(expected.roll) ||
        yaw != static_cast<std::uint16_t>(expected.yaw) ||
        throttle != expected.throttle || buttons != expected.buttons) {
      return false;
    }
  }
  return reader.finished();
}

} // namespace ac6::retail::detail
