#pragma once

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace ac6 {

struct CampaignResourceRoute {
  std::uint32_t selector{};
  std::uint32_t dpl_resource_id{};
  std::uint32_t data_table_entry{};
  bool valid() const noexcept {
    return selector != 0 && dpl_resource_id != 0 && data_table_entry != 0;
  }
  bool operator==(const CampaignResourceRoute&) const = default;
};

struct CampaignLoadout {
  std::uint32_t aircraft_id{};
  std::uint32_t weapon_id{};
  bool capability_data_valid{};
  bool valid() const noexcept {
    return aircraft_id != 0 && weapon_id != 0 && capability_data_valid;
  }
};

struct CampaignMissionSpec {
  std::uint32_t mission_id{};
  CampaignResourceRoute route;
  std::uint32_t objective_count{};
  std::vector<std::uint32_t> prerequisites;
  bool valid() const noexcept;
};

enum class CampaignMissionState : std::uint8_t {
  Locked,
  Available,
  Briefing,
  Active,
  Completed,
  Failed,
};

struct CampaignMissionStatus {
  std::uint32_t mission_id{};
  CampaignMissionState state{CampaignMissionState::Locked};
  std::uint32_t objective_mask{};
  CampaignLoadout loadout{};
};

struct CampaignSaveSnapshot {
  struct Record {
    std::uint32_t mission_id{};
    std::uint32_t objective_mask{};
    bool operator==(const Record&) const = default;
  };
  std::vector<Record> completed;
  bool operator==(const CampaignSaveSnapshot&) const = default;
};

class CampaignProgression final {
 public:
  bool add(CampaignMissionSpec spec);
  bool load_manifest(const std::filesystem::path& manifest);
  bool finalize() noexcept;
  bool set_loadout(std::uint32_t mission_id, CampaignLoadout loadout) noexcept;
  bool enter_briefing(std::uint32_t mission_id) noexcept;
  bool begin(std::uint32_t mission_id) noexcept;
  bool can_complete_objective(std::uint32_t mission_id,
                              std::uint32_t objective_index) const noexcept;
  bool complete_objective(std::uint32_t mission_id, std::uint32_t objective_index) noexcept;
  bool can_complete(std::uint32_t mission_id) const noexcept;
  bool complete(std::uint32_t mission_id) noexcept;
  bool fail(std::uint32_t mission_id) noexcept;
  bool is_available(std::uint32_t mission_id) const noexcept;
  const CampaignResourceRoute* route_for_selector(std::uint32_t selector) const noexcept;
  const CampaignMissionStatus* status(std::uint32_t mission_id) const noexcept;
  CampaignSaveSnapshot snapshot() const;
  bool restore(const CampaignSaveSnapshot& snapshot) noexcept;
  bool encode_snapshot(std::vector<std::uint8_t>& bytes) const;
  bool decode_snapshot(const std::vector<std::uint8_t>& bytes) noexcept;

 private:
  struct Entry {
    CampaignMissionSpec spec;
    CampaignMissionStatus status;
  };
  std::vector<Entry> entries_;
  bool finalized_{};

  Entry* find_entry(std::uint32_t mission_id) noexcept;
  const Entry* find_entry(std::uint32_t mission_id) const noexcept;
  bool prerequisites_complete(const Entry& entry) const noexcept;
  void refresh_unlocks() noexcept;
};

class CampaignSaveStore final {
 public:
  bool save(std::uint32_t slot, CampaignSaveSnapshot snapshot);
  const CampaignSaveSnapshot* load(std::uint32_t slot) const noexcept;
  bool write_file(const std::filesystem::path& path) const;
  bool read_file(const std::filesystem::path& path);

 private:
  std::unordered_map<std::uint32_t, CampaignSaveSnapshot> slots_;
};

}  // namespace ac6
