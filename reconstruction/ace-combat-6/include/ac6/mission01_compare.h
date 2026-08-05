#pragma once

#include "ac6/product_runtime.h"

#include <filesystem>
#include <string>
#include <vector>

namespace ac6 {

struct Mission01Checkpoint { WorldFrame frame; };

struct Mission01Thresholds {
  float position_m{0.5f};
  float orientation_deg{0.25f};
  float velocity_mps{0.5f};
  float camera_absolute{0.001f};
  float minimum_ssim{0.90f};
  float minimum_coverage_iou{0.95f};
  float maximum_depth_rmse{0.001f};
};

class Mission01Reference final {
 public:
  bool load(const std::filesystem::path& directory);
  const ReplayLog& replay() const noexcept { return replay_; }
  const std::vector<Mission01Checkpoint>& checkpoints() const noexcept { return checkpoints_; }
  const std::vector<std::uint8_t>& oracle_rgba() const noexcept { return oracle_rgba_; }
  const std::vector<float>& oracle_depth() const noexcept { return oracle_depth_; }
  std::uint32_t width() const noexcept { return width_; }
  std::uint32_t height() const noexcept { return height_; }

 private:
  ReplayLog replay_;
  std::vector<Mission01Checkpoint> checkpoints_;
  std::vector<std::uint8_t> oracle_rgba_;
  std::vector<float> oracle_depth_;
  std::uint32_t width_{};
  std::uint32_t height_{};
};

struct Mission01ComparisonResult {
  bool deterministic{};
  bool simulation_pass{};
  bool render_pass{};
  float maximum_position_error{};
  float maximum_orientation_error{};
  float maximum_camera_error{};
  float maximum_velocity_error{};
  float color_ssim{};
  float coverage_iou{};
  float depth_rmse{};
  std::string failure;
  bool passed() const noexcept { return deterministic && simulation_pass && render_pass; }
};

class Mission01Comparator final {
 public:
  explicit Mission01Comparator(Mission01Thresholds thresholds = {}) : thresholds_(thresholds) {}
  Mission01ComparisonResult compare(const Mission01Reference& reference,
                                    const std::vector<WorldFrame>& observed,
                                    const NativeRenderTarget& target,
                                    bool deterministic,
                                    const std::filesystem::path& output_directory) const;

 private:
  Mission01Thresholds thresholds_;
};

}  // namespace ac6
