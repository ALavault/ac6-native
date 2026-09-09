#pragma once

#include "ac6/retail_free_flight.h"
#include "ac6/retail_mission01_vulkan_scene.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace ac6::retail {

inline constexpr std::array<std::uint32_t, 8>
    kRetailFreeFlightReceiptSampleTicks{
        300U, 540U, 780U, 1020U, 1260U, 1500U, 1800U, 3600U};

bool retail_free_flight_receipt_sample_tick(std::uint32_t tick) noexcept;

// Deterministic, fail-closed evidence writer for the native N1b gate. The
// caller owns simulation and Vulkan; this object accepts only the eight fixed
// observations and writes no timestamp or host-dependent absolute path.
class RetailFreeFlightReceipt final {
 public:
  explicit RetailFreeFlightReceipt(std::filesystem::path output_directory);

  RetailFreeFlightReceipt(const RetailFreeFlightReceipt&) = delete;
  RetailFreeFlightReceipt& operator=(const RetailFreeFlightReceipt&) = delete;
  RetailFreeFlightReceipt(RetailFreeFlightReceipt&&) noexcept = default;
  RetailFreeFlightReceipt& operator=(RetailFreeFlightReceipt&&) noexcept =
      default;

  bool begin(const Sha256Digest& cache_index_sha256,
             const RetailMission01VulkanSceneReport& scene_report);
  bool observe(std::uint32_t tick, InputFrame input,
               const SimulationSnapshot& snapshot,
               std::uint64_t flight_state_digest,
               std::span<const std::uint8_t> rgba8);
  bool finalize(std::uint64_t final_tick,
                const Sha256Digest& replay_input_sha256);

  bool eligible() const noexcept { return eligible_; }
  const std::string& failure_detail() const noexcept { return failure_detail_; }
  const std::filesystem::path& manifest_path() const noexcept {
    return manifest_path_;
  }
  const Sha256Digest& manifest_sha256() const noexcept {
    return manifest_sha256_;
  }

 private:
  struct Sample final {
    std::uint32_t tick{};
    InputFrame input{};
    Sha256Digest snapshot_sha256{};
    std::uint64_t flight_state_digest{};
    Sha256Digest rgba_sha256{};
    std::array<float, 3> attitude{};
    float speed{};
    std::uint64_t changed_pixels{};
    std::uint64_t hud_green_pixels{};
    std::uint8_t luminance_min{};
    std::uint8_t luminance_max{};
    bool semantic_effect{};
    bool visual_effect{};
    std::string image_name;
  };

  bool fail(std::string detail);

  std::filesystem::path output_directory_;
  std::filesystem::path manifest_path_;
  Sha256Digest cache_index_sha256_{};
  RetailMission01VulkanSceneReport scene_report_{};
  std::vector<Sample> samples_;
  std::vector<std::uint8_t> previous_rgba8_;
  Sha256Digest manifest_sha256_{};
  std::string failure_detail_{"not_started"};
  bool begun_{};
  bool finalized_{};
  bool eligible_{};
};

}  // namespace ac6::retail
