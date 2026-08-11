#include "ac6/product_runtime.h"
#include "ac6/retail_frontend_resources.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>

namespace ac6 {
namespace {
std::uint32_t read_le_u32(const unsigned char* bytes) noexcept {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8u) |
         (static_cast<std::uint32_t>(bytes[2]) << 16u) |
         (static_cast<std::uint32_t>(bytes[3]) << 24u);
}
}  // namespace

bool FrontendController::advance() noexcept {
  switch (state_) {
    case FrontendState::Title: state_ = FrontendState::NewGame; break;
    case FrontendState::NewGame:
      if (campaign_ != nullptr &&
          (selected_mission_ == 0 || !campaign_->enter_briefing(selected_mission_))) return false;
      state_ = FrontendState::Briefing;
      break;
    case FrontendState::Briefing: state_ = FrontendState::Hangar; break;
    case FrontendState::Hangar:
      if (campaign_ != nullptr &&
          (selected_mission_ == 0 || !campaign_->begin(selected_mission_))) return false;
      state_ = FrontendState::Loading;
      break;
    case FrontendState::Loading: state_ = FrontendState::Mission; break;
    case FrontendState::Mission:
    case FrontendState::Pause:
    case FrontendState::Debrief:
    case FrontendState::Error:
      return false;
  }
  return true;
}

bool FrontendController::configure(FrontendSettings settings) noexcept {
  if (state_ != FrontendState::Title || !settings.valid()) return false;
  if (settings.language != FrontendLanguage::English) return false;
  settings_ = settings;
  return true;
}

bool FrontendController::configure(
    FrontendSettings settings,
    const retail::RetailFrontendResources& resources) noexcept {
  // The resource-qualified path is the only path allowed to select PAL
  // locale/difficulty/control variants.  It does not collapse them to the
  // legacy English/Normal/Normal diagnostic defaults.
  if (state_ != FrontendState::Title || !settings.valid() || !resources.complete() ||
      !resources.has_locale_slot(static_cast<std::uint32_t>(settings.language))) {
    return false;
  }
  settings_ = settings;
  return true;
}

bool FrontendController::dispatch(Event event) noexcept {
  switch (event.type) {
    case EventType::StartMission:
      return advance();
    case EventType::Pause:
      return pause();
    case EventType::Resume:
      return resume();
    case EventType::Abort:
      state_ = FrontendState::Title;
      selected_mission_ = 0;
      debrief_.reset();
      return true;
    default:
      return false;
  }
}

bool FrontendController::dispatch_buttons(const InputMappingDatabase& mappings,
                                          std::uint16_t buttons) noexcept {
  const InputBinding* binding = mappings.resolve(buttons);
  return binding != nullptr && dispatch({binding->event, 0});
}

bool FrontendController::select_mission(const MissionCatalog& catalog,
                                        std::uint32_t mission_id) noexcept {
  if (catalog.find(mission_id) == nullptr ||
      (campaign_ != nullptr && !campaign_->is_available(mission_id))) return false;
  selected_mission_ = mission_id;
  return true;
}

bool FrontendController::set_loadout(CampaignLoadout loadout) noexcept {
  return campaign_ != nullptr && state_ == FrontendState::Hangar && selected_mission_ != 0 &&
         campaign_->set_loadout(selected_mission_, loadout);
}

const MissionDefinition* FrontendController::mission_definition(
    const MissionCatalog& catalog) const noexcept {
  if (state_ != FrontendState::Mission || selected_mission_ == 0) return nullptr;
  return catalog.find(selected_mission_);
}

bool FrontendController::launch_selected(const MissionCatalog& catalog,
                                         const MissionLaunchDatabase& launches,
                                         MissionExecution& execution) const noexcept {
  if (state_ != FrontendState::Mission || selected_mission_ == 0 || execution.launched()) {
    return false;
  }
  const MissionDefinition* definition = catalog.find(selected_mission_);
  const MissionLaunchDefinition* launch = launches.find(selected_mission_);
  return definition != nullptr && launch != nullptr && definition->id == selected_mission_ &&
         execution.launch(*launch);
}

bool FrontendController::enter_debrief(const MissionExecution& execution) noexcept {
  if (state_ != FrontendState::Mission || selected_mission_ == 0 || !execution.launched()) {
    return false;
  }
  const MissionDebrief result = execution.debrief();
  if (result.mission_id != selected_mission_ || result.outcome == MissionOutcome::InProgress) {
    return false;
  }
  if (campaign_ != nullptr) {
    const CampaignMissionStatus* status = campaign_->status(selected_mission_);
    const CampaignMissionState expected = result.outcome == MissionOutcome::Success
        ? CampaignMissionState::Completed : CampaignMissionState::Failed;
    if (status == nullptr || status->state != expected) return false;
  }
  debrief_ = result;
  state_ = FrontendState::Debrief;
  return true;
}

bool FrontendController::return_to_campaign() noexcept {
  if (state_ != FrontendState::Debrief || !debrief_.has_value()) return false;
  state_ = FrontendState::NewGame;
  selected_mission_ = 0;
  debrief_.reset();
  return true;
}

bool FrontendController::pause() noexcept {
  if (state_ != FrontendState::Mission) return false;
  state_ = FrontendState::Pause;
  return true;
}

bool FrontendController::resume() noexcept {
  if (state_ != FrontendState::Pause) return false;
  state_ = FrontendState::Mission;
  return true;
}

bool FrontendController::fail() noexcept {
  if (state_ == FrontendState::Error) return false;
  state_ = FrontendState::Error;
  return true;
}

bool FrontendController::recover() noexcept {
  if (state_ != FrontendState::Error) return false;
  state_ = FrontendState::Title;
  selected_mission_ = 0;
  debrief_.reset();
  return true;
}

bool SaveStore::save(std::uint32_t slot, RuntimeSnapshot snapshot) {
  if (slot == 0 || snapshot.tick == 0 || !std::isfinite(snapshot.position_x) ||
      !std::isfinite(snapshot.position_y) || !std::isfinite(snapshot.position_z) ||
      !std::isfinite(snapshot.pitch) || !std::isfinite(snapshot.roll) ||
      !std::isfinite(snapshot.yaw) || !std::isfinite(snapshot.fixed_accumulator) ||
      snapshot.fixed_accumulator < 0.0f || snapshot.fixed_accumulator >= 1.0f / 60.0f) return false;
  slots_[slot] = snapshot;
  return true;
}

const RuntimeSnapshot* SaveStore::load(std::uint32_t slot) const noexcept {
  const auto it = slots_.find(slot);
  return it == slots_.end() ? nullptr : &it->second;
}

bool SaveStore::write_file(const std::filesystem::path& path) const {
  if (path.empty() || slots_.size() > 1024u) return false;
  std::vector<std::uint32_t> slot_ids;
  slot_ids.reserve(slots_.size());
  for (const auto& [slot, snapshot] : slots_) {
    (void)snapshot;
    if (slot == 0) return false;
    slot_ids.push_back(slot);
  }
  std::sort(slot_ids.begin(), slot_ids.end());
  std::filesystem::path temporary = path;
  temporary += ".tmp";
  std::error_code cleanup_error;
  std::filesystem::remove(temporary, cleanup_error);
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if (!output) {
    std::filesystem::remove(temporary, cleanup_error);
    return false;
  }
  const auto write_u32 = [&output](std::uint32_t value) {
    const unsigned char bytes[4] = {static_cast<unsigned char>(value & 0xffu),
                                    static_cast<unsigned char>((value >> 8u) & 0xffu),
                                    static_cast<unsigned char>((value >> 16u) & 0xffu),
                                    static_cast<unsigned char>((value >> 24u) & 0xffu)};
    output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
  };
  const auto write_u64 = [&output](std::uint64_t value) {
    unsigned char bytes[8]{};
    for (std::size_t i = 0; i < sizeof(bytes); ++i) bytes[i] =
        static_cast<unsigned char>((value >> (i * 8u)) & 0xffu);
    output.write(reinterpret_cast<const char*>(bytes), sizeof(bytes));
  };
  output.write("AC6SAVE\0", 8);
  write_u32(3u);
  write_u32(static_cast<std::uint32_t>(slot_ids.size()));
  for (const std::uint32_t slot : slot_ids) {
    const RuntimeSnapshot& snapshot = slots_.at(slot);
    write_u32(slot);
    write_u64(snapshot.tick);
    std::uint32_t raw{};
    std::memcpy(&raw, &snapshot.position_x, sizeof(raw)); write_u32(raw);
    std::memcpy(&raw, &snapshot.position_y, sizeof(raw)); write_u32(raw);
    std::memcpy(&raw, &snapshot.position_z, sizeof(raw)); write_u32(raw);
    std::memcpy(&raw, &snapshot.pitch, sizeof(raw)); write_u32(raw);
    std::memcpy(&raw, &snapshot.roll, sizeof(raw)); write_u32(raw);
    std::memcpy(&raw, &snapshot.yaw, sizeof(raw)); write_u32(raw);
    std::memcpy(&raw, &snapshot.fixed_accumulator, sizeof(raw)); write_u32(raw);
  }
  const bool written = static_cast<bool>(output);
  output.close();
  if (!written) {
    std::filesystem::remove(temporary, cleanup_error);
    return false;
  }
  std::error_code rename_error;
  std::filesystem::rename(temporary, path, rename_error);
  if (rename_error) {
    std::filesystem::remove(temporary, cleanup_error);
    return false;
  }
  return true;
}

bool SaveStore::read_file(const std::filesystem::path& path) {
  if (path.empty()) return false;
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  char magic[8]{};
  input.read(magic, sizeof(magic));
  if (!input || std::memcmp(magic, "AC6SAVE\0", 8) != 0) return false;
  unsigned char header[8]{};
  input.read(reinterpret_cast<char*>(header), sizeof(header));
  if (!input || (read_le_u32(header) != 1u && read_le_u32(header) != 2u &&
                 read_le_u32(header) != 3u) ||
      read_le_u32(header + 4) > 1024u) return false;
  const std::uint32_t version = read_le_u32(header);
  const std::uint32_t count = read_le_u32(header + 4);
  std::unordered_map<std::uint32_t, RuntimeSnapshot> loaded;
  for (std::uint32_t i = 0; i < count; ++i) {
    constexpr std::size_t kV1RecordSize = 24;
    constexpr std::size_t kV2RecordSize = 36;
    constexpr std::size_t kV3RecordSize = 40;
    std::array<unsigned char, kV3RecordSize> record{};
    const std::size_t record_size = version == 1u ? kV1RecordSize :
                                    (version == 2u ? kV2RecordSize : kV3RecordSize);
    input.read(reinterpret_cast<char*>(record.data()), static_cast<std::streamsize>(record_size));
    if (!input) return false;
    const std::uint32_t slot = read_le_u32(record.data());
    if (slot == 0 || loaded.find(slot) != loaded.end()) return false;
    const std::uint64_t tick = [&record]() {
      std::uint64_t value = 0;
      for (std::size_t j = 0; j < 8; ++j) value |= static_cast<std::uint64_t>(record[4 + j]) << (j * 8u);
      return value;
    }();
    RuntimeSnapshot snapshot{tick, 0.0f, 0.0f, 0.0f};
    const auto read_float = [&record](std::size_t offset, float& value) {
      const std::uint32_t raw = read_le_u32(record.data() + offset);
      std::memcpy(&value, &raw, sizeof(value));
      return std::isfinite(value);
    };
    if (!read_float(12, snapshot.position_x) || !read_float(16, snapshot.position_y) ||
        !read_float(20, snapshot.position_z) || snapshot.tick == 0) return false;
    if (version == 2u && (!read_float(24, snapshot.pitch) || !read_float(28, snapshot.roll) ||
                          !read_float(32, snapshot.yaw))) return false;
    if (version == 3u && (!read_float(24, snapshot.pitch) || !read_float(28, snapshot.roll) ||
                          !read_float(32, snapshot.yaw) || !read_float(36, snapshot.fixed_accumulator) ||
                          snapshot.fixed_accumulator < 0.0f || snapshot.fixed_accumulator >= 1.0f / 60.0f)) return false;
    loaded.emplace(slot, snapshot);
  }
  char extra = 0;
  if (input.read(&extra, 1)) return false;
  if (!input.eof()) return false;
  slots_ = std::move(loaded);
  return true;
}


}  // namespace ac6
