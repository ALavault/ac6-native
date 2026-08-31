#pragma once

// Small, platform-neutral offline service contracts.  They are deliberately
// independent from the Xbox SDK and do not open sockets or depend on retail
// media.  Runtime integration remains a later gate.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace ac6::native {

enum class ServiceError : std::uint8_t {
  kNone,
  kOffline,
  kInvalidPath,
  kNotFound,
  kBusy,
  kCorrupt,
  kPollMismatch,
  kReplayIncomplete,
  kRingFull,
  kRingEmpty,
};

class OfflineNetwork final {
 public:
  [[nodiscard]] ServiceError connect(std::string_view) const noexcept {
    return ServiceError::kOffline;
  }
  [[nodiscard]] ServiceError send(std::span<const std::uint8_t>) const noexcept {
    return ServiceError::kOffline;
  }
  [[nodiscard]] ServiceError receive(std::span<std::uint8_t>) const noexcept {
    return ServiceError::kOffline;
  }
};

class HandleTable final {
 public:
  [[nodiscard]] std::uint32_t allocate() noexcept;
  [[nodiscard]] bool close(std::uint32_t handle) noexcept;
  [[nodiscard]] bool contains(std::uint32_t handle) const noexcept;

 private:
  std::unordered_set<std::uint32_t> live_;
  std::uint32_t next_{0x100u};
};

class AutoResetEvent final {
 public:
  void signal() noexcept { signaled_ = true; }
  void reset() noexcept { signaled_ = false; }
  [[nodiscard]] bool consume() noexcept {
    if (!signaled_) return false;
    signaled_ = false;
    return true;
  }

 private:
  bool signaled_{};
};

class XenonTimebase final {
 public:
  static constexpr std::uint64_t kFrequency = 50'000'000u;
  static constexpr std::uint64_t kTickRate = 60u;

  void set_tick(std::uint64_t tick) noexcept { tick_ = tick; }
  void advance() noexcept { ++tick_; }
  [[nodiscard]] std::uint64_t tick() const noexcept { return tick_; }
  [[nodiscard]] std::uint64_t now() const noexcept {
    return (tick_ * kFrequency) / kTickRate;
  }

 private:
  std::uint64_t tick_{};
};

class ConfinedVfs final {
 public:
  explicit ConfinedVfs(std::filesystem::path root);

  [[nodiscard]] bool valid() const noexcept { return valid_root_; }
  [[nodiscard]] ServiceError resolve(std::string_view relative,
                                     std::filesystem::path& result) const;
  [[nodiscard]] ServiceError read(std::string_view relative,
                                  std::vector<std::uint8_t>& bytes) const;

 private:
  std::filesystem::path root_;
  bool valid_root_{};
};

class AtomicSaveStore final {
 public:
  explicit AtomicSaveStore(std::filesystem::path root);

  [[nodiscard]] bool valid() const noexcept { return !root_.empty(); }

  [[nodiscard]] ServiceError write(std::string_view name,
                                   std::span<const std::uint8_t> bytes);
  [[nodiscard]] ServiceError read(std::string_view name,
                                  std::vector<std::uint8_t>& bytes) const;

 private:
  std::filesystem::path root_;
  std::uint64_t temporary_counter_{};
};

struct ReplaySample final {
  std::uint64_t tick{};
  std::uint32_t user{};
  std::uint32_t buttons{};
  std::int16_t left_x{};
  std::int16_t left_y{};
  std::int16_t right_x{};
  std::int16_t right_y{};
};

class StrictInputReplay final {
 public:
  explicit StrictInputReplay(std::vector<ReplaySample> samples);

  [[nodiscard]] ServiceError poll(std::uint64_t tick, std::uint32_t user,
                                  ReplaySample& sample) noexcept;
  [[nodiscard]] ServiceError finalize() const noexcept;
  [[nodiscard]] std::size_t consumed() const noexcept { return cursor_; }
  [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }

 private:
  std::vector<ReplaySample> samples_;
  std::size_t cursor_{};
};

class XmaDoubleBuffer final {
 public:
  explicit XmaDoubleBuffer(std::size_t slot_capacity);

  [[nodiscard]] ServiceError push(std::span<const std::uint8_t> packet);
  [[nodiscard]] ServiceError pop(std::vector<std::uint8_t>& packet);
  [[nodiscard]] std::size_t queued() const noexcept { return queued_; }
  [[nodiscard]] std::size_t capacity() const noexcept { return slot_capacity_; }

 private:
  std::vector<std::uint8_t> slots_[2];
  std::size_t slot_capacity_{};
  std::size_t write_slot_{};
  std::size_t read_slot_{};
  std::size_t queued_{};
};

}  // namespace ac6::native
