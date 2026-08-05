#include "ac6/campaign_progression.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <charconv>
#include <fstream>
#include <string_view>

namespace ac6 {

namespace {

constexpr std::array<char, 8> kMagic{{'A', 'C', '6', 'C', 'A', 'M', 'P', '\0'}};
constexpr std::array<char, 8> kSaveMagic{{'A', 'C', '6', 'C', 'S', 'A', 'V', '\0'}};

void write_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
  bytes.push_back(static_cast<std::uint8_t>(value & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
  bytes.push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
}

bool read_u32(const std::vector<std::uint8_t>& bytes, std::size_t& offset,
              std::uint32_t& value) noexcept {
  if (offset > bytes.size() || bytes.size() - offset < 4) return false;
  value = static_cast<std::uint32_t>(bytes[offset]) |
          (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
          (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
          (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
  offset += 4;
  return true;
}

bool valid_snapshot(const CampaignSaveSnapshot& snapshot) noexcept {
  if (snapshot.completed.size() > 1024) return false;
  std::uint32_t previous = 0;
  for (const CampaignSaveSnapshot::Record record : snapshot.completed) {
    if (record.mission_id == 0 || record.mission_id <= previous) return false;
    previous = record.mission_id;
  }
  return true;
}

}  // namespace

bool CampaignMissionSpec::valid() const noexcept {
  if (mission_id == 0 || !route.valid() || objective_count == 0 || objective_count > 32) {
    return false;
  }
  for (std::size_t index = 0; index < prerequisites.size(); ++index) {
    const std::uint32_t prerequisite = prerequisites[index];
    if (prerequisite == 0 || prerequisite == mission_id ||
        std::find(prerequisites.begin() + static_cast<std::ptrdiff_t>(index) + 1,
                  prerequisites.end(), prerequisite) != prerequisites.end()) {
      return false;
    }
  }
  return true;
}

CampaignProgression::Entry* CampaignProgression::find_entry(std::uint32_t mission_id) noexcept {
  const auto it = std::find_if(entries_.begin(), entries_.end(),
                               [mission_id](const Entry& entry) {
                                 return entry.spec.mission_id == mission_id;
                               });
  return it == entries_.end() ? nullptr : &*it;
}

const CampaignProgression::Entry* CampaignProgression::find_entry(
    std::uint32_t mission_id) const noexcept {
  const auto it = std::find_if(entries_.begin(), entries_.end(),
                               [mission_id](const Entry& entry) {
                                 return entry.spec.mission_id == mission_id;
                               });
  return it == entries_.end() ? nullptr : &*it;
}

bool CampaignProgression::add(CampaignMissionSpec spec) {
  if (finalized_ || !spec.valid() || find_entry(spec.mission_id) != nullptr) return false;
  for (const Entry& entry : entries_) {
    if (entry.spec.route.selector == spec.route.selector) {
      return false;
    }
  }
  const std::uint32_t mission_id = spec.mission_id;
  entries_.push_back({std::move(spec), {mission_id, CampaignMissionState::Locked, 0, {}}});
  return true;
}

bool CampaignProgression::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  CampaignProgression loaded;
  std::string line;
  const auto parse_u32 = [](std::string_view text, std::uint32_t& value) noexcept {
    if (text.empty()) return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
  };
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::array<std::string_view, 6> fields{};
    std::string_view remaining(line);
    for (std::size_t index = 0; index < fields.size(); ++index) {
      const std::size_t tab = remaining.find('\t');
      if (index + 1 == fields.size()) {
        fields[index] = remaining;
      } else {
        if (tab == std::string_view::npos) return false;
        fields[index] = remaining.substr(0, tab);
        remaining.remove_prefix(tab + 1);
      }
      if (fields[index].empty()) return false;
    }
    CampaignMissionSpec spec;
    if (!parse_u32(fields[0], spec.mission_id) ||
        !parse_u32(fields[1], spec.route.selector) ||
        !parse_u32(fields[2], spec.route.dpl_resource_id) ||
        !parse_u32(fields[3], spec.route.data_table_entry) ||
        !parse_u32(fields[4], spec.objective_count)) return false;
    if (fields[5] != "-") {
      std::string_view prerequisites = fields[5];
      while (!prerequisites.empty()) {
        const std::size_t comma = prerequisites.find(',');
        const std::string_view token = prerequisites.substr(0, comma);
        std::uint32_t prerequisite = 0;
        if (!parse_u32(token, prerequisite)) return false;
        spec.prerequisites.push_back(prerequisite);
        if (comma == std::string_view::npos) break;
        prerequisites.remove_prefix(comma + 1);
      }
    }
    if (!loaded.add(std::move(spec))) return false;
  }
  if (!loaded.finalize()) return false;
  *this = std::move(loaded);
  return true;
}

bool CampaignProgression::finalize() noexcept {
  if (finalized_ || entries_.empty()) return false;
  for (const Entry& entry : entries_) {
    for (const std::uint32_t prerequisite : entry.spec.prerequisites) {
      if (find_entry(prerequisite) == nullptr) return false;
    }
  }
  std::vector<std::uint32_t> visiting;
  std::vector<std::uint32_t> visited;
  std::function<bool(std::uint32_t)> visit = [&](std::uint32_t id) {
    if (std::find(visited.begin(), visited.end(), id) != visited.end()) return true;
    if (std::find(visiting.begin(), visiting.end(), id) != visiting.end()) return false;
    const Entry* current = find_entry(id);
    if (current == nullptr) return false;
    visiting.push_back(id);
    for (const std::uint32_t prerequisite : current->spec.prerequisites) {
      if (!visit(prerequisite)) return false;
    }
    visiting.pop_back();
    visited.push_back(id);
    return true;
  };
  for (const Entry& entry : entries_) {
    if (!visit(entry.spec.mission_id)) return false;
  }
  finalized_ = true;
  refresh_unlocks();
  return true;
}

bool CampaignProgression::set_loadout(std::uint32_t mission_id, CampaignLoadout loadout) noexcept {
  Entry* entry = find_entry(mission_id);
  if (!finalized_ || entry == nullptr || !loadout.valid() ||
      (entry->status.state != CampaignMissionState::Available &&
       entry->status.state != CampaignMissionState::Briefing)) return false;
  entry->status.loadout = loadout;
  return true;
}

bool CampaignProgression::prerequisites_complete(const Entry& entry) const noexcept {
  return std::all_of(entry.spec.prerequisites.begin(), entry.spec.prerequisites.end(),
                     [this](std::uint32_t prerequisite) {
                       const Entry* required = find_entry(prerequisite);
                       return required != nullptr &&
                              required->status.state == CampaignMissionState::Completed;
                     });
}

void CampaignProgression::refresh_unlocks() noexcept {
  for (Entry& entry : entries_) {
    if (entry.status.state == CampaignMissionState::Locked &&
        prerequisites_complete(entry)) {
      entry.status.state = CampaignMissionState::Available;
    }
  }
}

bool CampaignProgression::enter_briefing(std::uint32_t mission_id) noexcept {
  Entry* entry = find_entry(mission_id);
  if (!finalized_ || entry == nullptr || entry->status.state != CampaignMissionState::Available) {
    return false;
  }
  entry->status.state = CampaignMissionState::Briefing;
  return true;
}

bool CampaignProgression::begin(std::uint32_t mission_id) noexcept {
  Entry* entry = find_entry(mission_id);
  if (!finalized_ || entry == nullptr || entry->status.state != CampaignMissionState::Briefing ||
      !entry->status.loadout.valid()) return false;
  entry->status.state = CampaignMissionState::Active;
  return true;
}

bool CampaignProgression::can_complete_objective(std::uint32_t mission_id,
                                                 std::uint32_t objective_index) const noexcept {
  const Entry* entry = find_entry(mission_id);
  return finalized_ && entry != nullptr && entry->status.state == CampaignMissionState::Active &&
         objective_index < entry->spec.objective_count;
}

bool CampaignProgression::complete_objective(std::uint32_t mission_id,
                                             std::uint32_t objective_index) noexcept {
  Entry* entry = find_entry(mission_id);
  if (!finalized_ || entry == nullptr || entry->status.state != CampaignMissionState::Active ||
      objective_index >= entry->spec.objective_count) return false;
  entry->status.objective_mask |= (1u << objective_index);
  return true;
}

bool CampaignProgression::can_complete(std::uint32_t mission_id) const noexcept {
  const Entry* entry = find_entry(mission_id);
  return finalized_ && entry != nullptr && entry->status.state == CampaignMissionState::Active &&
         entry->status.objective_mask ==
             (0xffffffffu >> (32u - entry->spec.objective_count));
}

bool CampaignProgression::complete(std::uint32_t mission_id) noexcept {
  Entry* entry = find_entry(mission_id);
  if (!finalized_ || entry == nullptr || entry->status.state != CampaignMissionState::Active ||
      entry->status.objective_mask != (0xffffffffu >> (32u - entry->spec.objective_count))) {
    return false;
  }
  entry->status.state = CampaignMissionState::Completed;
  refresh_unlocks();
  return true;
}

bool CampaignProgression::fail(std::uint32_t mission_id) noexcept {
  Entry* entry = find_entry(mission_id);
  if (!finalized_ || entry == nullptr || entry->status.state != CampaignMissionState::Active) {
    return false;
  }
  entry->status.state = CampaignMissionState::Failed;
  return true;
}

bool CampaignProgression::is_available(std::uint32_t mission_id) const noexcept {
  const Entry* entry = find_entry(mission_id);
  return entry != nullptr && entry->status.state == CampaignMissionState::Available;
}

const CampaignResourceRoute* CampaignProgression::route_for_selector(
    std::uint32_t selector) const noexcept {
  const auto it = std::find_if(entries_.begin(), entries_.end(),
                               [selector](const Entry& entry) {
                                 return entry.spec.route.selector == selector;
                               });
  return it == entries_.end() ? nullptr : &it->spec.route;
}

const CampaignMissionStatus* CampaignProgression::status(std::uint32_t mission_id) const noexcept {
  const Entry* entry = find_entry(mission_id);
  return entry == nullptr ? nullptr : &entry->status;
}

CampaignSaveSnapshot CampaignProgression::snapshot() const {
  CampaignSaveSnapshot result;
  for (const Entry& entry : entries_) {
    if (entry.status.state == CampaignMissionState::Completed) {
      result.completed.push_back({entry.spec.mission_id, entry.status.objective_mask});
    }
  }
  std::sort(result.completed.begin(), result.completed.end(),
            [](const auto& left, const auto& right) { return left.mission_id < right.mission_id; });
  return result;
}

bool CampaignProgression::restore(const CampaignSaveSnapshot& snapshot) noexcept {
  if (!finalized_) return false;
  std::uint32_t previous = 0;
  for (const CampaignSaveSnapshot::Record record : snapshot.completed) {
    const Entry* entry = find_entry(record.mission_id);
    if (entry == nullptr || record.mission_id <= previous ||
        record.objective_mask != (0xffffffffu >> (32u - entry->spec.objective_count))) {
      return false;
    }
    previous = record.mission_id;
  }
  for (Entry& entry : entries_) {
    entry.status.state = CampaignMissionState::Locked;
    entry.status.objective_mask = 0;
  }
  for (const CampaignSaveSnapshot::Record record : snapshot.completed) {
    Entry* entry = find_entry(record.mission_id);
    entry->status.state = CampaignMissionState::Completed;
    entry->status.objective_mask = record.objective_mask;
  }
  refresh_unlocks();
  return true;
}

bool CampaignProgression::encode_snapshot(std::vector<std::uint8_t>& bytes) const {
  const CampaignSaveSnapshot current = snapshot();
  if (current.completed.size() > 1024) return false;
  bytes.assign(kMagic.begin(), kMagic.end());
  write_u32(bytes, 1);
  write_u32(bytes, static_cast<std::uint32_t>(current.completed.size()));
  for (const auto record : current.completed) {
    write_u32(bytes, record.mission_id);
    write_u32(bytes, record.objective_mask);
  }
  return true;
}

bool CampaignProgression::decode_snapshot(const std::vector<std::uint8_t>& bytes) noexcept {
  if (bytes.size() < kMagic.size() + 8 ||
      !std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) return false;
  std::size_t offset = kMagic.size();
  std::uint32_t version = 0;
  std::uint32_t count = 0;
  if (!read_u32(bytes, offset, version) || !read_u32(bytes, offset, count) || version != 1 ||
      count > 1024 || bytes.size() - offset != static_cast<std::size_t>(count) * 8) {
    return false;
  }
  CampaignSaveSnapshot parsed;
  parsed.completed.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    CampaignSaveSnapshot::Record record;
    if (!read_u32(bytes, offset, record.mission_id) || !read_u32(bytes, offset, record.objective_mask)) {
      return false;
    }
    parsed.completed.push_back(record);
  }
  return restore(parsed);
}

bool CampaignSaveStore::save(std::uint32_t slot, CampaignSaveSnapshot snapshot) {
  if (slot == 0 || !valid_snapshot(snapshot)) return false;
  slots_[slot] = std::move(snapshot);
  return true;
}

const CampaignSaveSnapshot* CampaignSaveStore::load(std::uint32_t slot) const noexcept {
  const auto it = slots_.find(slot);
  return it == slots_.end() ? nullptr : &it->second;
}

bool CampaignSaveStore::write_file(const std::filesystem::path& path) const {
  if (path.empty() || slots_.size() > 1024) return false;
  const std::filesystem::path temporary = path.string() + ".tmp";
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output.write(kSaveMagic.data(), static_cast<std::streamsize>(kSaveMagic.size()));
  const auto write_file_u32 = [&output](std::uint32_t value) {
    const char bytes[4] = {static_cast<char>(value & 0xffu),
                           static_cast<char>((value >> 8u) & 0xffu),
                           static_cast<char>((value >> 16u) & 0xffu),
                           static_cast<char>((value >> 24u) & 0xffu)};
    output.write(bytes, sizeof(bytes));
  };
  write_file_u32(1);
  write_file_u32(static_cast<std::uint32_t>(slots_.size()));
  std::vector<std::uint32_t> slots;
  slots.reserve(slots_.size());
  for (const auto& [slot, snapshot] : slots_) {
    if (!valid_snapshot(snapshot)) return false;
    slots.push_back(slot);
  }
  std::sort(slots.begin(), slots.end());
  for (const std::uint32_t slot : slots) {
    const CampaignSaveSnapshot& snapshot = slots_.at(slot);
    write_file_u32(slot);
    write_file_u32(static_cast<std::uint32_t>(snapshot.completed.size()));
    for (const CampaignSaveSnapshot::Record record : snapshot.completed) {
      write_file_u32(record.mission_id);
      write_file_u32(record.objective_mask);
    }
  }
  if (!output) {
    output.close();
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return false;
  }
  output.close();
  std::error_code error;
  std::filesystem::rename(temporary, path, error);
  if (error) {
    std::filesystem::remove(temporary, error);
    return false;
  }
  return true;
}

bool CampaignSaveStore::read_file(const std::filesystem::path& path) {
  if (path.empty()) return false;
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  char magic[8]{};
  input.read(magic, sizeof(magic));
  if (!input || std::memcmp(magic, kSaveMagic.data(), kSaveMagic.size()) != 0) return false;
  const auto read_file_u32 = [&input](std::uint32_t& value) {
    unsigned char bytes[4]{};
    input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (!input) return false;
    value = static_cast<std::uint32_t>(bytes[0]) |
            (static_cast<std::uint32_t>(bytes[1]) << 8u) |
            (static_cast<std::uint32_t>(bytes[2]) << 16u) |
            (static_cast<std::uint32_t>(bytes[3]) << 24u);
    return true;
  };
  std::uint32_t version = 0;
  std::uint32_t count = 0;
  if (!read_file_u32(version) || !read_file_u32(count) || version != 1 || count > 1024) {
    return false;
  }
  std::unordered_map<std::uint32_t, CampaignSaveSnapshot> loaded;
  for (std::uint32_t index = 0; index < count; ++index) {
    std::uint32_t slot = 0;
    std::uint32_t record_count = 0;
    if (!read_file_u32(slot) || !read_file_u32(record_count) || slot == 0 ||
        loaded.find(slot) != loaded.end() || record_count > 1024) return false;
    CampaignSaveSnapshot snapshot;
    snapshot.completed.reserve(record_count);
    for (std::uint32_t record_index = 0; record_index < record_count; ++record_index) {
      CampaignSaveSnapshot::Record record;
      if (!read_file_u32(record.mission_id) || !read_file_u32(record.objective_mask)) return false;
      snapshot.completed.push_back(record);
    }
    if (!valid_snapshot(snapshot) || !loaded.emplace(slot, std::move(snapshot)).second) return false;
  }
  char extra = 0;
  if (input.read(&extra, 1)) return false;
  if (!input.eof()) return false;
  slots_ = std::move(loaded);
  return true;
}

}  // namespace ac6
