#include "ac6/product_runtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <utility>

namespace ac6 {

namespace {

constexpr std::array<char, 8> kMagic{{'A', 'C', '6', 'S', 'E', 'S', 'S', '\0'}};

bool valid_campaign(const CampaignSaveSnapshot& snapshot) noexcept {
  if (snapshot.completed.size() > 1024) return false;
  std::uint32_t previous = 0;
  for (const CampaignSaveSnapshot::Record record : snapshot.completed) {
    if (record.mission_id == 0 || record.mission_id <= previous) return false;
    previous = record.mission_id;
  }
  return true;
}

bool valid_flight(const RuntimeSnapshot& flight) noexcept {
  return flight.tick != 0 && std::isfinite(flight.position_x) &&
         std::isfinite(flight.position_y) && std::isfinite(flight.position_z) &&
         std::isfinite(flight.pitch) && std::isfinite(flight.roll) &&
         std::isfinite(flight.yaw) && std::isfinite(flight.fixed_accumulator) &&
         flight.fixed_accumulator >= 0.0f && flight.fixed_accumulator < 1.0f / 60.0f;
}

bool valid_session(const SessionSaveSnapshot& snapshot) noexcept {
  return snapshot.mission_id != 0 && valid_flight(snapshot.flight) &&
         valid_campaign(snapshot.campaign);
}

void write_u32(std::ostream& output, std::uint32_t value) {
  const char bytes[4] = {static_cast<char>(value & 0xffu),
                         static_cast<char>((value >> 8u) & 0xffu),
                         static_cast<char>((value >> 16u) & 0xffu),
                         static_cast<char>((value >> 24u) & 0xffu)};
  output.write(bytes, sizeof(bytes));
}

void write_u64(std::ostream& output, std::uint64_t value) {
  for (unsigned int index = 0; index < 8; ++index) {
    const char byte = static_cast<char>((value >> (index * 8u)) & 0xffu);
    output.write(&byte, 1);
  }
}

void write_f32(std::ostream& output, float value) {
  std::uint32_t raw = 0;
  std::memcpy(&raw, &value, sizeof(raw));
  write_u32(output, raw);
}

bool read_u32(std::istream& input, std::uint32_t& value) {
  unsigned char bytes[4]{};
  input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
  if (!input) return false;
  value = static_cast<std::uint32_t>(bytes[0]) |
          (static_cast<std::uint32_t>(bytes[1]) << 8u) |
          (static_cast<std::uint32_t>(bytes[2]) << 16u) |
          (static_cast<std::uint32_t>(bytes[3]) << 24u);
  return true;
}

bool read_u64(std::istream& input, std::uint64_t& value) {
  unsigned char bytes[8]{};
  input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
  if (!input) return false;
  value = 0;
  for (unsigned int index = 0; index < 8; ++index) {
    value |= static_cast<std::uint64_t>(bytes[index]) << (index * 8u);
  }
  return true;
}

bool read_f32(std::istream& input, float& value) {
  std::uint32_t raw = 0;
  if (!read_u32(input, raw)) return false;
  std::memcpy(&value, &raw, sizeof(value));
  return std::isfinite(value);
}

void write_flight(std::ostream& output, const RuntimeSnapshot& flight) {
  write_u64(output, flight.tick);
  write_f32(output, flight.position_x);
  write_f32(output, flight.position_y);
  write_f32(output, flight.position_z);
  write_f32(output, flight.pitch);
  write_f32(output, flight.roll);
  write_f32(output, flight.yaw);
  write_f32(output, flight.fixed_accumulator);
}

bool read_flight(std::istream& input, RuntimeSnapshot& flight) {
  return read_u64(input, flight.tick) && read_f32(input, flight.position_x) &&
         read_f32(input, flight.position_y) && read_f32(input, flight.position_z) &&
         read_f32(input, flight.pitch) && read_f32(input, flight.roll) &&
         read_f32(input, flight.yaw) && read_f32(input, flight.fixed_accumulator) &&
         valid_flight(flight);
}

}  // namespace

bool SessionSaveStore::save(std::uint32_t slot, SessionSaveSnapshot snapshot) {
  if (slot == 0 || !valid_session(snapshot)) return false;
  slots_[slot] = std::move(snapshot);
  return true;
}

const SessionSaveSnapshot* SessionSaveStore::load(std::uint32_t slot) const noexcept {
  const auto it = slots_.find(slot);
  return it == slots_.end() ? nullptr : &it->second;
}

bool SessionSaveStore::write_file(const std::filesystem::path& path) const {
  if (path.empty() || slots_.size() > 1024) return false;
  const std::filesystem::path temporary = path.string() + ".tmp";
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
  write_u32(output, 1);
  write_u32(output, static_cast<std::uint32_t>(slots_.size()));
  std::vector<std::uint32_t> slots;
  slots.reserve(slots_.size());
  for (const auto& [slot, snapshot] : slots_) {
    if (!valid_session(snapshot)) return false;
    slots.push_back(slot);
  }
  std::sort(slots.begin(), slots.end());
  for (const std::uint32_t slot : slots) {
    const SessionSaveSnapshot& snapshot = slots_.at(slot);
    write_u32(output, slot);
    write_u32(output, snapshot.mission_id);
    write_flight(output, snapshot.flight);
    write_u32(output, static_cast<std::uint32_t>(snapshot.campaign.completed.size()));
    for (const CampaignSaveSnapshot::Record record : snapshot.campaign.completed) {
      write_u32(output, record.mission_id);
      write_u32(output, record.objective_mask);
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

bool SessionSaveStore::read_file(const std::filesystem::path& path) {
  if (path.empty()) return false;
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  char magic[8]{};
  input.read(magic, sizeof(magic));
  if (!input || std::memcmp(magic, kMagic.data(), kMagic.size()) != 0) return false;
  std::uint32_t version = 0;
  std::uint32_t count = 0;
  if (!read_u32(input, version) || !read_u32(input, count) || version != 1 || count > 1024) {
    return false;
  }
  std::unordered_map<std::uint32_t, SessionSaveSnapshot> loaded;
  for (std::uint32_t index = 0; index < count; ++index) {
    std::uint32_t slot = 0;
    SessionSaveSnapshot snapshot;
    std::uint32_t record_count = 0;
    if (!read_u32(input, slot) || !read_u32(input, snapshot.mission_id) || slot == 0 ||
        loaded.find(slot) != loaded.end() || !read_flight(input, snapshot.flight) ||
        !read_u32(input, record_count) || record_count > 1024) return false;
    snapshot.campaign.completed.reserve(record_count);
    for (std::uint32_t record_index = 0; record_index < record_count; ++record_index) {
      CampaignSaveSnapshot::Record record;
      if (!read_u32(input, record.mission_id) || !read_u32(input, record.objective_mask)) {
        return false;
      }
      snapshot.campaign.completed.push_back(record);
    }
    if (!valid_session(snapshot) || !loaded.emplace(slot, std::move(snapshot)).second) return false;
  }
  char extra = 0;
  if (input.read(&extra, 1)) return false;
  if (!input.eof()) return false;
  slots_ = std::move(loaded);
  return true;
}

}  // namespace ac6
