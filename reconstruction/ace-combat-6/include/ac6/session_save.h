#pragma once

#include "ac6/product_runtime.h"

namespace ac6 {

class SaveStore final {
 public:
  bool save(std::uint32_t slot, RuntimeSnapshot snapshot);
  const RuntimeSnapshot* load(std::uint32_t slot) const noexcept;
  bool write_file(const std::filesystem::path& path) const;
  bool read_file(const std::filesystem::path& path);

 private:
  std::unordered_map<std::uint32_t, RuntimeSnapshot> slots_;
};

struct SessionSaveSnapshot {
  std::uint32_t mission_id{};
  // Zero denotes a legacy/manifest save. Retail commands fill this with the
  // sealed content-index digest before writing a product save.
  Sha256Digest content_index_sha256{};
  RuntimeSnapshot flight{};
  CampaignSaveSnapshot campaign;
  std::optional<MissionExecution::Checkpoint> checkpoint;
  bool operator==(const SessionSaveSnapshot&) const = default;
};

class SessionSaveStore final {
 public:
  bool save(std::uint32_t slot, SessionSaveSnapshot snapshot);
  const SessionSaveSnapshot* load(std::uint32_t slot) const noexcept;
  bool write_file(const std::filesystem::path& path) const;
  bool read_file(const std::filesystem::path& path);

 private:
  std::unordered_map<std::uint32_t, SessionSaveSnapshot> slots_;
};

class ReplayLog final {
 public:
  void append(InputFrame input) { frames_.push_back(input); }
  const std::vector<InputFrame>& frames() const noexcept { return frames_; }
  void clear() noexcept { frames_.clear(); }
  bool write_file(const std::filesystem::path& path) const;
  bool read_file(const std::filesystem::path& path);

 private:
  std::vector<InputFrame> frames_;
};

}  // namespace ac6
