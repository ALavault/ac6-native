#include "ac6/product_runtime.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace ac6 {

namespace {
template <std::size_t FieldCount>
bool parse_tsv_fields(std::string_view line,
                      std::array<std::string_view, FieldCount>& fields) noexcept {
  std::size_t start = 0;
  for (std::size_t index = 0; index < FieldCount; ++index) {
    if (start > line.size()) return false;
    const std::size_t tab = line.find('\t', start);
    if (index + 1 == FieldCount) {
      if (tab != std::string_view::npos) return false;
      fields[index] = line.substr(start);
    } else {
      if (tab == std::string_view::npos) return false;
      fields[index] = line.substr(start, tab - start);
      start = tab + 1;
    }
    if (fields[index].empty()) return false;
  }
  return true;
}

bool parse_u32(std::string_view text, std::uint32_t& value) noexcept {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}
bool parse_u64(std::string_view text, std::uint64_t& value) noexcept {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}
bool parse_f32(std::string_view text, float& value) noexcept {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && std::isfinite(value);
}
bool parse_bool01(std::string_view text, bool& value) noexcept {
  if (text == "0") { value = false; return true; }
  if (text == "1") { value = true; return true; }
  return false;
}
bool parse_objective_condition(std::string_view text, ObjectiveCondition& condition) noexcept {
  if (text == "manual") { condition = ObjectiveCondition::Manual; return true; }
  if (text == "destroy_unit") { condition = ObjectiveCondition::DestroyUnit; return true; }
  if (text == "protect_unit") { condition = ObjectiveCondition::ProtectUnit; return true; }
  return false;
}
bool parse_hex_u32(std::string_view text, std::uint32_t& value) noexcept {
  if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text.remove_prefix(2);
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);
  return !text.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}
bool parse_hex_u64(std::string_view text, std::uint64_t& value) noexcept {
  if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text.remove_prefix(2);
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);
  return !text.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

class Sha256 final {
 public:
  void update(const unsigned char* data, std::size_t size) noexcept {
    bit_length_ += static_cast<std::uint64_t>(size) * 8u;
    while (size != 0) {
      const std::size_t count = std::min(size, block_.size() - block_size_);
      std::memcpy(block_.data() + block_size_, data, count);
      block_size_ += count;
      data += count;
      size -= count;
      if (block_size_ == block_.size()) {
        transform(block_.data());
        block_size_ = 0;
      }
    }
  }

  std::array<unsigned char, 32> digest() const noexcept {
    Sha256 copy = *this;
    copy.finish();
    std::array<unsigned char, 32> result{};
    for (std::size_t index = 0; index < copy.state_.size(); ++index) {
      const std::uint32_t word = copy.state_[index];
      result[index * 4u] = static_cast<unsigned char>(word >> 24u);
      result[index * 4u + 1u] = static_cast<unsigned char>(word >> 16u);
      result[index * 4u + 2u] = static_cast<unsigned char>(word >> 8u);
      result[index * 4u + 3u] = static_cast<unsigned char>(word);
    }
    return result;
  }

 private:
  static constexpr std::array<std::uint32_t, 64> kRoundConstants{
      0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
      0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
      0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
      0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
      0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
      0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
      0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
      0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
      0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
      0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
      0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
      0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
      0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
      0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
      0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
      0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

  static std::uint32_t rotate_right(std::uint32_t value, unsigned count) noexcept {
    return (value >> count) | (value << (32u - count));
  }

  void transform(const unsigned char* block) noexcept {
    std::array<std::uint32_t, 64> words{};
    for (std::size_t index = 0; index < 16; ++index) {
      const std::size_t offset = index * 4u;
      words[index] = (static_cast<std::uint32_t>(block[offset]) << 24u) |
                     (static_cast<std::uint32_t>(block[offset + 1u]) << 16u) |
                     (static_cast<std::uint32_t>(block[offset + 2u]) << 8u) |
                     static_cast<std::uint32_t>(block[offset + 3u]);
    }
    for (std::size_t index = 16; index < words.size(); ++index) {
      const std::uint32_t s0 = rotate_right(words[index - 15u], 7u) ^
                               rotate_right(words[index - 15u], 18u) ^
                               (words[index - 15u] >> 3u);
      const std::uint32_t s1 = rotate_right(words[index - 2u], 17u) ^
                               rotate_right(words[index - 2u], 19u) ^
                               (words[index - 2u] >> 10u);
      words[index] = words[index - 16u] + s0 + words[index - 7u] + s1;
    }
    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];
    for (std::size_t index = 0; index < words.size(); ++index) {
      const std::uint32_t s1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^ rotate_right(e, 25u);
      const std::uint32_t choose = (e & f) ^ ((~e) & g);
      const std::uint32_t temporary1 = h + s1 + choose + kRoundConstants[index] + words[index];
      const std::uint32_t s0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^ rotate_right(a, 22u);
      const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t temporary2 = s0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + temporary1;
      d = c;
      c = b;
      b = a;
      a = temporary1 + temporary2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  void finish() noexcept {
    block_[block_size_++] = 0x80u;
    if (block_size_ > 56u) {
      std::fill(block_.begin() + static_cast<std::ptrdiff_t>(block_size_), block_.end(), 0u);
      transform(block_.data());
      block_size_ = 0;
    }
    std::fill(block_.begin() + static_cast<std::ptrdiff_t>(block_size_), block_.begin() + 56, 0u);
    for (std::size_t index = 0; index < 8; ++index) {
      block_[56u + index] = static_cast<unsigned char>(bit_length_ >> (56u - index * 8u));
    }
    transform(block_.data());
    block_size_ = 0;
  }

  std::array<std::uint32_t, 8> state_{
      0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
  std::array<unsigned char, 64> block_{};
  std::size_t block_size_{};
  std::uint64_t bit_length_{};
};

bool file_sha256(const std::filesystem::path& path, std::string& digest) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  Sha256 sha;
  std::array<unsigned char, 64 * 1024> bytes{};
  while (input) {
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    const std::streamsize count = input.gcount();
    if (count > 0) sha.update(bytes.data(), static_cast<std::size_t>(count));
  }
  if (!input.eof()) return false;
  constexpr char hex[] = "0123456789abcdef";
  digest.clear();
  digest.reserve(64);
  for (const unsigned char byte : sha.digest()) {
    digest.push_back(hex[byte >> 4u]);
    digest.push_back(hex[byte & 0x0fu]);
  }
  return true;
}

bool equal_hex(std::string_view left, std::string_view right) noexcept {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    const auto lower = [](char value) {
      return static_cast<char>(value >= 'A' && value <= 'F' ? value + ('a' - 'A') : value);
    };
    if (lower(left[index]) != lower(right[index])) return false;
  }
  return true;
}

bool parse_asset_ids(std::string_view text, std::vector<AssetId>& asset_ids) {
  while (!text.empty()) {
    const auto comma = text.find(',');
    const auto token = text.substr(0, comma);
    AssetId id{};
    if (token.empty() || !parse_u32(token, id) || id == 0 ||
        std::find(asset_ids.begin(), asset_ids.end(), id) != asset_ids.end()) {
      return false;
    }
    asset_ids.push_back(id);
    if (comma == std::string_view::npos) break;
    text.remove_prefix(comma + 1);
  }
  return !asset_ids.empty();
}

bool parse_units(std::string_view text, std::vector<UnitRecord>& units) {
  while (!text.empty()) {
    const auto comma = text.find(',');
    const auto token = text.substr(0, comma);
    const auto first = token.find(':');
    const auto second = first == std::string_view::npos ? std::string_view::npos :
        token.find(':', first + 1);
    if (first == std::string_view::npos || second == std::string_view::npos ||
        token.find(':', second + 1) != std::string_view::npos) {
      return false;
    }
    UnitRecord unit;
    if (!parse_u32(token.substr(0, first), unit.id) ||
        !parse_u32(token.substr(first + 1, second - first - 1), unit.owner) ||
        !parse_u32(token.substr(second + 1), unit.asset)) {
      return false;
    }
    unit.active = false;
    if (unit.id == 0 || unit.asset == 0 || unit.owner == unit.id ||
        std::find_if(units.begin(), units.end(), [unit](const UnitRecord& existing) {
          return existing.id == unit.id;
        }) != units.end()) {
      return false;
    }
    units.push_back(unit);
    if (comma == std::string_view::npos) break;
    text.remove_prefix(comma + 1);
  }
  return !units.empty();
}

bool parse_weapons(std::string_view text, std::vector<WeaponDefinition>& weapons) {
  if (text.empty()) return false;
  while (!text.empty()) {
    const auto comma = text.find(',');
    const auto token = text.substr(0, comma);
    std::array<std::string_view, 5> fields{};
    std::string_view remaining = token;
    for (std::size_t index = 0; index < fields.size(); ++index) {
      const auto colon = remaining.find(':');
      if (index + 1 == fields.size()) {
        fields[index] = remaining;
      } else {
        if (colon == std::string_view::npos) return false;
        fields[index] = remaining.substr(0, colon);
        remaining.remove_prefix(colon + 1);
      }
      if (fields[index].empty()) return false;
    }
    WeaponDefinition weapon;
    if (!parse_u32(fields[0], weapon.id) || !parse_f32(fields[1], weapon.damage) ||
        !parse_f32(fields[2], weapon.projectile_speed) ||
        !parse_f32(fields[3], weapon.cooldown) || !parse_f32(fields[4], weapon.max_range) ||
        std::find_if(weapons.begin(), weapons.end(), [weapon](const auto& existing) {
          return existing.id == weapon.id;
        }) != weapons.end()) return false;
    weapons.push_back(weapon);
    if (comma == std::string_view::npos) break;
    text.remove_prefix(comma + 1);
  }
  return !weapons.empty();
}

}  // namespace

bool MissionAssetDatabase::add(AssetRecord record) {
  if (record.id == 0 || record.relative_path.empty() || record.sha256.empty()) {
    return false;
  }
  return records_.emplace(record.id, std::move(record)).second;
}

bool MissionAssetDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionAssetDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::array<std::size_t, 4> tabs{};
    std::size_t tab_count = 0;
    std::size_t search_from = 0;
    while (tab_count < tabs.size()) {
      const auto tab = line.find('\t', search_from);
      if (tab == std::string::npos) break;
      tabs[tab_count++] = tab;
      search_from = tab + 1;
    }
    if (tab_count < 2 || tab_count > tabs.size()) return false;

    AssetRecord record;
    const auto line_view = std::string_view(line);
    if (!parse_u32(line_view.substr(0, tabs[0]), record.id)) return false;
    record.relative_path = line.substr(tabs[0] + 1, tabs[1] - tabs[0] - 1);
    if (tab_count == 2) {
      record.sha256 = line.substr(tabs[1] + 1);
    } else {
      record.sha256 = line.substr(tabs[1] + 1, tabs[2] - tabs[1] - 1);
      const auto size_end = tab_count == 4 ? tabs[3] : line.size();
      if (!parse_u64(line_view.substr(tabs[2] + 1, size_end - tabs[2] - 1),
                     record.byte_size) ||
          record.byte_size == 0) {
        return false;
      }
      if (tab_count == 4) {
        const auto dependencies = line_view.substr(tabs[3] + 1);
        if (dependencies != "-" && !parse_asset_ids(dependencies, record.dependencies)) {
          return false;
        }
      }
    }
    if (!loaded.add(std::move(record))) return false;
  }
  records_ = std::move(loaded.records_);
  return true;
}

bool MissionAssetDatabase::load_qualified_manifest(const std::filesystem::path& manifest) {
  MissionAssetDatabase loaded;
  if (!loaded.load_manifest(manifest)) return false;
  for (const auto& [id, record] : loaded.records_) {
    (void)id;
    if (record.sha256.size() != 64 ||
        !std::all_of(record.sha256.begin(), record.sha256.end(), [](char value) {
          return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') ||
                 (value >= 'A' && value <= 'F');
      })) {
      return false;
    }
  }

  bool extended = false;
  for (const auto& [id, record] : loaded.records_) {
    (void)id;
    extended = extended || record.byte_size != 0 || !record.dependencies.empty();
  }
  if (extended) {
    for (const auto& [id, record] : loaded.records_) {
      if (record.byte_size == 0) return false;
      for (const AssetId dependency : record.dependencies) {
        if (dependency == id || loaded.records_.find(dependency) == loaded.records_.end()) {
          return false;
        }
      }
      const std::filesystem::path relative(record.relative_path);
      if (relative.is_absolute() || std::any_of(relative.begin(), relative.end(),
                                                 [](const auto& component) {
                                                   return component == "..";
                                                 })) {
        return false;
      }
      const std::filesystem::path resolved = manifest.parent_path() / relative;
      std::error_code error;
      const std::uintmax_t size = std::filesystem::file_size(resolved, error);
      if (error || size != record.byte_size) return false;
      std::string digest;
      if (!file_sha256(resolved, digest) || !equal_hex(record.sha256, digest)) return false;
    }

    std::unordered_map<AssetId, std::uint8_t> marks;
    std::function<bool(AssetId)> acyclic = [&](AssetId id) {
      auto& mark = marks[id];
      if (mark == 1) return false;
      if (mark == 2) return true;
      mark = 1;
      const auto record = loaded.records_.find(id);
      if (record == loaded.records_.end()) return false;
      for (const AssetId dependency : record->second.dependencies) {
        if (!acyclic(dependency)) return false;
      }
      mark = 2;
      return true;
    };
    for (const auto& [id, record] : loaded.records_) {
      (void)record;
      if (!acyclic(id)) return false;
    }
  }
  records_ = std::move(loaded.records_);
  return true;
}

const AssetRecord* MissionAssetDatabase::resolve(AssetId id) const noexcept {
  const auto it = records_.find(id);
  return it == records_.end() ? nullptr : &it->second;
}

bool MissionLaunchDatabase::add(MissionLaunchDefinition definition) {
  if (definition.mission_id == 0 || definition.player_entity == 0 || definition.units.empty()) {
    return false;
  }
  bool has_player = false;
  for (std::size_t i = 0; i < definition.units.size(); ++i) {
    const UnitRecord& unit = definition.units[i];
    if (unit.id == 0 || unit.asset == 0 || unit.owner == unit.id) return false;
    if (unit.id == definition.player_entity) has_player = true;
    if (std::find_if(definition.units.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                     definition.units.end(), [unit](const UnitRecord& existing) {
                       return existing.id == unit.id;
                     }) != definition.units.end()) {
      return false;
    }
  }
  for (std::size_t i = 0; i < definition.weapons.size(); ++i) {
    if (!definition.weapons[i].valid() ||
        std::find_if(definition.weapons.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                     definition.weapons.end(), [weapon = definition.weapons[i]](
                         const WeaponDefinition& existing) {
                       return existing.id == weapon.id;
                     }) != definition.weapons.end()) return false;
  }
  if (!has_player) return false;
  return launches_.emplace(definition.mission_id, std::move(definition)).second;
}

bool MissionLaunchDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionLaunchDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    if (first == std::string::npos || second == std::string::npos ||
        (third != std::string::npos && line.find('\t', third + 1) != std::string::npos)) {
      return false;
    }
    MissionLaunchDefinition definition;
    if (!parse_u32(std::string_view(line).substr(0, first), definition.mission_id) ||
        !parse_u32(std::string_view(line).substr(first + 1, second - first - 1),
                   definition.player_entity) ||
        !parse_units(std::string_view(line).substr(second + 1,
                                                   (third == std::string::npos ? line.size() : third) -
                                                       second - 1), definition.units) ||
        (third != std::string::npos &&
         !parse_weapons(std::string_view(line).substr(third + 1), definition.weapons)) ||
        !loaded.add(std::move(definition))) {
      return false;
    }
  }
  launches_ = std::move(loaded.launches_);
  return true;
}

const MissionLaunchDefinition* MissionLaunchDatabase::find(
    std::uint32_t mission_id) const noexcept {
  const auto it = launches_.find(mission_id);
  return it == launches_.end() ? nullptr : &it->second;
}

bool MissionManifestLoader::load_paths(const std::filesystem::path& manifest,
                                       MissionManifestPaths& paths) const {
  if (manifest.empty()) return false;
  std::ifstream input(manifest);
  if (!input) return false;
  MissionManifestPaths loaded;
  const std::filesystem::path root = manifest.parent_path();
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto tab = line.find('\t');
    if (tab == std::string::npos || line.find('\t', tab + 1) != std::string::npos) return false;
    const std::string key = line.substr(0, tab);
    const std::string value = line.substr(tab + 1);
    if (value.empty()) return false;
    std::filesystem::path resolved(value);
    if (resolved.is_relative()) resolved = root / resolved;
    if (key == "campaign" && loaded.campaign.empty()) loaded.campaign = resolved;
    else if (key == "catalog" && loaded.catalog.empty()) loaded.catalog = resolved;
    else if (key == "assets" && loaded.assets.empty()) loaded.assets = resolved;
    else if (key == "launches" && loaded.launches.empty()) loaded.launches = resolved;
    else if (key == "input" && loaded.input.empty()) loaded.input = resolved;
    else if (key == "controls" && loaded.controls.empty()) loaded.controls = resolved;
    else if (key == "objectives" && loaded.objectives.empty()) loaded.objectives = resolved;
    else if (key == "radios" && loaded.radios.empty()) loaded.radios = resolved;
    else if (key == "waves" && loaded.waves.empty()) loaded.waves = resolved;
    else if (key == "ai" && loaded.ai.empty()) loaded.ai = resolved;
    else if (key == "sequence" && loaded.sequence.empty()) loaded.sequence = resolved;
    else if (key == "render" && loaded.render.empty()) loaded.render = resolved;
    else if (key == "drawables" && loaded.drawables.empty()) loaded.drawables = resolved;
    else if (key == "transforms" && loaded.transforms.empty()) loaded.transforms = resolved;
    else if (key == "materials" && loaded.materials.empty()) loaded.materials = resolved;
    else if (key == "textures" && loaded.textures.empty()) loaded.textures = resolved;
    else if (key == "shaders" && loaded.shaders.empty()) loaded.shaders = resolved;
    else if (key == "targets" && loaded.targets.empty()) loaded.targets = resolved;
    else if (key == "passes" && loaded.passes.empty()) loaded.passes = resolved;
    else if (key == "resolves" && loaded.resolves.empty()) loaded.resolves = resolved;
    else if (key == "buffers" && loaded.buffers.empty()) loaded.buffers = resolved;
    else if (key == "camera" && loaded.camera.empty()) loaded.camera = resolved;
    else return false;
  }
  if (!loaded.valid()) return false;
  paths = std::move(loaded);
  return true;
}

bool MissionManifestLoader::load_campaign(const std::filesystem::path& manifest,
                                           CampaignProgression& campaign) const {
  MissionManifestPaths paths;
  if (!load_paths(manifest, paths) || paths.campaign.empty()) return false;
  CampaignProgression loaded;
  if (!loaded.load_manifest(paths.campaign)) return false;
  campaign = std::move(loaded);
  return true;
}

bool MissionManifestLoader::load_runtime(const std::filesystem::path& manifest,
                                         MissionCatalog& catalog,
                                         MissionAssetDatabase& assets,
                                         MissionLaunchDatabase& launches) const {
  MissionRuntimeServices services;
  return load_runtime(manifest, catalog, assets, launches, services);
}

bool MissionManifestLoader::load_runtime(const std::filesystem::path& manifest,
                                         MissionCatalog& catalog,
                                         MissionAssetDatabase& assets,
                                         MissionLaunchDatabase& launches,
                                         MissionRuntimeServices& services) const {
  MissionManifestPaths paths;
  if (!load_paths(manifest, paths)) return false;
  MissionCatalog loaded_catalog;
  MissionAssetDatabase loaded_assets;
  MissionLaunchDatabase loaded_launches;
  MissionRuntimeServices loaded_services;
  if (!loaded_catalog.load_manifest(paths.catalog) ||
      !loaded_assets.load_qualified_manifest(paths.assets) ||
      !loaded_launches.load_manifest(paths.launches)) return false;
  if (!paths.input.empty()) {
    if (!loaded_services.input.load_manifest(paths.input)) return false;
    loaded_services.has_input = true;
  }
  if (!paths.objectives.empty()) {
    if (!loaded_services.objectives.load_manifest(paths.objectives)) return false;
    loaded_services.has_objectives = true;
  }
  if (!paths.radios.empty()) {
    if (!loaded_services.radios.load_manifest(paths.radios)) return false;
    loaded_services.has_radios = true;
  }
  if (!paths.waves.empty()) {
    if (!loaded_services.waves.load_manifest(paths.waves)) return false;
    loaded_services.has_waves = true;
  }
  if (!paths.ai.empty()) {
    if (!loaded_services.ai.load_manifest(paths.ai)) return false;
    loaded_services.has_ai = true;
  }
  if (!paths.sequence.empty()) {
    if (!loaded_services.sequence.load_manifest(paths.sequence)) return false;
    loaded_services.has_sequence = true;
  }
  if (!paths.campaign.empty()) {
    if (!loaded_services.campaign.load_manifest(paths.campaign)) return false;
    loaded_services.has_campaign = true;
  }
  catalog = std::move(loaded_catalog);
  assets = std::move(loaded_assets);
  launches = std::move(loaded_launches);
  services = std::move(loaded_services);
  return true;
}

bool MissionManifestLoader::load_input(const std::filesystem::path& manifest,
                                       InputMappingDatabase& input) const {
  MissionManifestPaths paths;
  if (!load_paths(manifest, paths) || paths.input.empty()) return false;
  InputMappingDatabase loaded;
  if (!loaded.load_manifest(paths.input)) return false;
  input = std::move(loaded);
  return true;
}

bool MissionCameraDefinition::valid() const noexcept {
  if (mission_id == 0) return false;
  for (const float value : clip_rows) if (!std::isfinite(value)) return false;
  return true;
}

bool MissionCameraDatabase::add(MissionCameraDefinition definition) {
  if (!definition.valid() || find(definition.mission_id) != nullptr) return false;
  cameras_.push_back(std::move(definition));
  return true;
}

bool MissionCameraDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionCameraDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::array<std::string_view, 20> fields{};
    std::size_t start = 0;
    std::size_t field_count = 0;
    while (field_count < fields.size()) {
      const std::size_t tab = line.find('\t', start);
      if (tab == std::string::npos) {
        fields[field_count++] = std::string_view(line).substr(start);
        break;
      } else {
        fields[field_count++] = std::string_view(line).substr(start, tab - start);
        start = tab + 1;
      }
    }
    if (field_count < 17 || field_count > 19) return false;
    if (field_count == fields.size() && line.find('\t', start) != std::string::npos) return false;
    MissionCameraDefinition definition;
    if (!parse_u32(fields[0], definition.mission_id)) return false;
    for (std::size_t i = 0; i < definition.clip_rows.size(); ++i) {
      if (!parse_f32(fields[i + 1], definition.clip_rows[i])) return false;
    }
    for (std::size_t field = 17; field < field_count; ++field) {
      if (fields[field] == "qualified") definition.qualified = true;
      else if (fields[field] == "column_major") definition.column_major = true;
      else return false;
    }
    if (!loaded.add(std::move(definition))) return false;
  }
  cameras_ = std::move(loaded.cameras_);
  return !cameras_.empty();
}

const MissionCameraDefinition* MissionCameraDatabase::find(std::uint32_t mission_id) const noexcept {
  const auto it = std::find_if(cameras_.begin(), cameras_.end(),
                               [mission_id](const MissionCameraDefinition& camera) {
                                 return camera.mission_id == mission_id;
                               });
  return it == cameras_.end() ? nullptr : &*it;
}

bool MissionManifestLoader::load_camera(const std::filesystem::path& manifest,
                                        MissionCameraDatabase& cameras) const {
  MissionManifestPaths paths;
  if (!load_paths(manifest, paths) || paths.camera.empty()) return false;
  MissionCameraDatabase loaded;
  if (!loaded.load_manifest(paths.camera)) return false;
  cameras = std::move(loaded);
  return true;
}

bool MissionManifestLoader::load_render(const std::filesystem::path& manifest,
                                        MissionRenderDatabase& render,
                                        MissionDrawableDatabase& drawables,
                                        MissionTransformDatabase& transforms,
                                        MissionMaterialDatabase& materials,
                                        MissionTextureDatabase& textures,
                                        ShaderPermutationDatabase& shaders,
                                        MissionRenderTargetDatabase& targets,
                                        MissionRenderPassDatabase& passes,
                                        MissionRenderResolveDatabase& resolves,
                                        QualifiedBufferDatabase& buffers) const {
  MissionManifestPaths paths;
  if (!load_paths(manifest, paths) || !paths.render_valid()) return false;
  MissionRenderDatabase loaded_render;
  MissionDrawableDatabase loaded_drawables;
  MissionTransformDatabase loaded_transforms;
  MissionMaterialDatabase loaded_materials;
  MissionTextureDatabase loaded_textures;
  ShaderPermutationDatabase loaded_shaders;
  MissionRenderTargetDatabase loaded_targets;
  MissionRenderPassDatabase loaded_passes;
  MissionRenderResolveDatabase loaded_resolves;
  QualifiedBufferDatabase loaded_buffers;
  if (!loaded_render.load_manifest(paths.render) ||
      !loaded_drawables.load_manifest(paths.drawables) ||
      !loaded_transforms.load_manifest(paths.transforms) ||
      !loaded_materials.load_manifest(paths.materials) ||
      !loaded_textures.load_manifest(paths.textures) ||
      !loaded_shaders.load_manifest(paths.shaders) ||
      !loaded_targets.load_manifest(paths.targets) ||
      !loaded_passes.load_manifest(paths.passes) ||
      !loaded_resolves.load_manifest(paths.resolves) ||
      !loaded_buffers.load_manifest(paths.buffers)) return false;
  render = std::move(loaded_render);
  drawables = std::move(loaded_drawables);
  transforms = std::move(loaded_transforms);
  materials = std::move(loaded_materials);
  textures = std::move(loaded_textures);
  shaders = std::move(loaded_shaders);
  targets = std::move(loaded_targets);
  passes = std::move(loaded_passes);
  resolves = std::move(loaded_resolves);
  buffers = std::move(loaded_buffers);
  return true;
}

bool MissionManifestLoader::load_render(const std::filesystem::path& manifest,
                                        MissionRenderDatabase& render,
                                        MissionDrawableDatabase& drawables,
                                        MissionTransformDatabase& transforms,
                                        MissionMaterialDatabase& materials,
                                        MissionTextureDatabase& textures,
                                        ShaderPermutationDatabase& shaders,
                                        MissionRenderTargetDatabase& targets,
                                        MissionRenderPassDatabase& passes,
                                        MissionRenderResolveDatabase& resolves,
                                        QualifiedBufferDatabase& buffers,
                                        NativeGeometryDatabase& geometries) const {
  MissionRenderDatabase loaded_render;
  MissionDrawableDatabase loaded_drawables;
  MissionTransformDatabase loaded_transforms;
  MissionMaterialDatabase loaded_materials;
  MissionTextureDatabase loaded_textures;
  ShaderPermutationDatabase loaded_shaders;
  MissionRenderTargetDatabase loaded_targets;
  MissionRenderPassDatabase loaded_passes;
  MissionRenderResolveDatabase loaded_resolves;
  QualifiedBufferDatabase loaded_buffers;
  if (!load_render(manifest, loaded_render, loaded_drawables, loaded_transforms,
                   loaded_materials, loaded_textures, loaded_shaders, loaded_targets,
                   loaded_passes, loaded_resolves, loaded_buffers)) return false;
  NativeGeometryDatabase loaded_geometries;
  std::unordered_set<std::string> loaded_buffer_ids;
  for (const auto& [mission_id, definition] : loaded_render.definitions()) {
    for (const AssetId asset : definition.asset_ids) {
      for (const MissionDrawable* drawable : loaded_drawables.find_by_asset(mission_id, asset)) {
        if (drawable == nullptr || !loaded_buffer_ids.insert(drawable->buffer_id).second) continue;
        if (!loaded_buffers.verify(drawable->buffer_id) ||
            !loaded_geometries.load_verified(*drawable, loaded_buffers)) return false;
      }
    }
  }
  render = std::move(loaded_render);
  drawables = std::move(loaded_drawables);
  transforms = std::move(loaded_transforms);
  materials = std::move(loaded_materials);
  textures = std::move(loaded_textures);
  shaders = std::move(loaded_shaders);
  targets = std::move(loaded_targets);
  passes = std::move(loaded_passes);
  resolves = std::move(loaded_resolves);
  buffers = std::move(loaded_buffers);
  geometries = std::move(loaded_geometries);
  return true;
}

bool configure_mission_launch(const MissionLaunchDefinition& launch, UnitRegistry& units,
                              MissionScenario& scenario) noexcept {
  if (launch.mission_id == 0 || launch.mission_id != scenario.mission_id() ||
      launch.player_entity == 0 || launch.units.empty()) {
    return false;
  }
  for (UnitRecord unit : launch.units) {
    if (!units.register_unit(unit) || !units.activate(unit.id)) return false;
  }
  return scenario.bind_player(units, launch.player_entity);
}


}  // namespace ac6
