#include "ac6/product_runtime.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <string_view>
#include <utility>
#include <unordered_set>

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
}

std::uint32_t stable_hash32(std::string_view text) noexcept {
  std::uint32_t hash = 2166136261u;
  for (const char byte : text) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= 16777619u;
  }
  return hash;
}

bool file_fnv64(const std::filesystem::path& path, std::uint64_t& hash) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  std::array<char, 64 * 1024> bytes{};
  std::uint64_t value = 1469598103934665603ull;
  while (input) {
    input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    const std::streamsize count = input.gcount();
    for (std::streamsize i = 0; i < count; ++i) {
      value ^= static_cast<unsigned char>(bytes[static_cast<std::size_t>(i)]);
      value *= 1099511628211ull;
    }
  }
  if (!input.eof()) return false;
  hash = value;
  return true;
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

std::uint16_t read_le_u16(const unsigned char* bytes) noexcept {
  return static_cast<std::uint16_t>(bytes[0]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8u);
}

std::uint32_t read_le_u32(const unsigned char* bytes) noexcept {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8u) |
         (static_cast<std::uint32_t>(bytes[2]) << 16u) |
         (static_cast<std::uint32_t>(bytes[3]) << 24u);
}

float read_le_f32(const unsigned char* bytes) noexcept {
  const std::uint32_t raw = read_le_u32(bytes);
  float value = 0.0f;
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

bool read_exact_at(std::ifstream& input, std::uint64_t offset, unsigned char* bytes,
                   std::size_t size) {
  if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max())) {
    return false;
  }
  input.clear();
  input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
  if (!input) return false;
  input.read(reinterpret_cast<char*>(bytes), static_cast<std::streamsize>(size));
  return input.good();
}

struct Vec3 {
  float x{};
  float y{};
  float z{};
};

struct ScreenPoint {
  std::uint32_t x{};
  std::uint32_t y{};
  float depth{};
  float ndc_x{};
  float ndc_y{};
  float ndc_z{};
};

float dot(Vec3 a, Vec3 b) noexcept {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b) noexcept {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

bool normalize(Vec3& value) noexcept {
  const float length_squared = dot(value, value);
  if (!std::isfinite(length_squared) || length_squared <= 0.000001f) return false;
  const float inverse_length = 1.0f / std::sqrt(length_squared);
  value.x *= inverse_length;
  value.y *= inverse_length;
  value.z *= inverse_length;
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

struct NativeCameraProjection {
  Vec3 origin;
  Vec3 right;
  Vec3 up;
  Vec3 forward;
  float aspect{};
  float focal{};
};

bool make_projection(const WorldFrame& frame, std::uint32_t width, std::uint32_t height,
                     NativeCameraProjection& projection) noexcept {
  if (width == 0 || height == 0 ||
      !std::isfinite(frame.camera_x) || !std::isfinite(frame.camera_y) ||
      !std::isfinite(frame.camera_z) || !std::isfinite(frame.camera_target_x) ||
      !std::isfinite(frame.camera_target_y) || !std::isfinite(frame.camera_target_z)) {
    return false;
  }
  projection.origin = {frame.camera_x, frame.camera_y, frame.camera_z};
  projection.forward = {frame.camera_target_x - frame.camera_x,
                        frame.camera_target_y - frame.camera_y,
                        frame.camera_target_z - frame.camera_z};
  if (!normalize(projection.forward)) return false;
  projection.right = cross(projection.forward, {0.0f, 1.0f, 0.0f});
  if (!normalize(projection.right)) {
    projection.right = cross(projection.forward, {0.0f, 0.0f, 1.0f});
    if (!normalize(projection.right)) return false;
  }
  projection.up = cross(projection.right, projection.forward);
  if (!normalize(projection.up)) return false;
  projection.aspect = static_cast<float>(width) / static_cast<float>(height);
  projection.focal = 1.7320508075688772f;
  return std::isfinite(projection.aspect) && projection.aspect > 0.0f;
}

bool project_clip_point(const MissionCameraDefinition& camera, Vec3 world,
                        std::uint32_t width, std::uint32_t height,
                        ScreenPoint& screen, bool clip_to_viewport = true) noexcept {
  const auto& m = camera.clip_rows;
  const float x = camera.column_major
      ? m[0] * world.x + m[4] * world.y + m[8] * world.z + m[12]
      : m[0] * world.x + m[1] * world.y + m[2] * world.z + m[3];
  const float y = camera.column_major
      ? m[1] * world.x + m[5] * world.y + m[9] * world.z + m[13]
      : m[4] * world.x + m[5] * world.y + m[6] * world.z + m[7];
  const float z = camera.column_major
      ? m[2] * world.x + m[6] * world.y + m[10] * world.z + m[14]
      : m[8] * world.x + m[9] * world.y + m[10] * world.z + m[11];
  const float w = camera.column_major
      ? m[3] * world.x + m[7] * world.y + m[11] * world.z + m[15]
      : m[12] * world.x + m[13] * world.y + m[14] * world.z + m[15];
  if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
      !std::isfinite(w) || w <= 0.000001f) return false;
  const float ndc_x = x / w;
  const float ndc_y = y / w;
  const float ndc_z = z / w;
  if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y) || !std::isfinite(ndc_z)) return false;
  if (clip_to_viewport && (ndc_x < -1.0f || ndc_x > 1.0f || ndc_y < -1.0f || ndc_y > 1.0f ||
                           ndc_z < -1.0f || ndc_z > 1.0f)) return false;
  const float safe_x = std::clamp(ndc_x, -1.0f, 1.0f);
  const float safe_y = std::clamp(ndc_y, -1.0f, 1.0f);
  screen.x = std::min(width - 1u, static_cast<std::uint32_t>((safe_x * 0.5f + 0.5f) * width));
  screen.y = std::min(height - 1u, static_cast<std::uint32_t>((0.5f - safe_y * 0.5f) * height));
  screen.ndc_x = ndc_x;
  screen.ndc_y = ndc_y;
  screen.ndc_z = ndc_z;
  // c218–c221 are Xenos-style homogeneous rows (depth range -1..1),
  // whereas the native depth plane is normalized to [0,1].
  screen.depth = std::clamp(ndc_z * 0.5f + 0.5f, 0.0f, 1.0f);
  return true;
}

bool project_point(const NativeCameraProjection& projection, Vec3 world,
                   std::uint32_t width, std::uint32_t height,
                   ScreenPoint& screen, bool clip_to_viewport = true) noexcept {
  const Vec3 relative{world.x - projection.origin.x, world.y - projection.origin.y,
                      world.z - projection.origin.z};
  const float view_x = dot(relative, projection.right);
  const float view_y = dot(relative, projection.up);
  const float view_z = dot(relative, projection.forward);
  constexpr float near_plane = 0.1f;
  if (!std::isfinite(view_x) || !std::isfinite(view_y) || !std::isfinite(view_z) ||
      view_z <= near_plane) {
    return false;
  }
  const float ndc_x = (view_x * projection.focal) / (view_z * projection.aspect);
  const float ndc_y = (view_y * projection.focal) / view_z;
  if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y) ||
      (clip_to_viewport && (ndc_x < -1.0f || ndc_x > 1.0f || ndc_y < -1.0f || ndc_y > 1.0f))) {
    return false;
  }
  const float safe_x = std::clamp(ndc_x, -1.0f, 1.0f);
  const float safe_y = std::clamp(ndc_y, -1.0f, 1.0f);
  screen.x = std::min(width - 1u,
                      static_cast<std::uint32_t>((safe_x * 0.5f + 0.5f) *
                                                 static_cast<float>(width)));
  screen.y = std::min(height - 1u,
                      static_cast<std::uint32_t>((0.5f - safe_y * 0.5f) *
                                                 static_cast<float>(height)));
  // The fallback camera has no explicit far plane in its manifest. Keep its
  // depth contract identical to the qualified clip camera and normalize once
  // here, at the projection boundary.
  constexpr float far_plane = 4096.0f;
  screen.depth = std::clamp(view_z / far_plane, 0.0f, 1.0f);
  screen.ndc_x = ndc_x;
  screen.ndc_y = ndc_y;
  screen.ndc_z = screen.depth * 2.0f - 1.0f;
  return true;
}

MissionFamily parse_family(std::string_view family) noexcept {
  if (family == "air_intercept") return MissionFamily::AirIntercept;
  if (family == "strike") return MissionFamily::Strike;
  if (family == "escort") return MissionFamily::Escort;
  return MissionFamily::Unknown;
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

bool MissionCatalog::add(MissionDefinition definition) {
  if (definition.id == 0 || definition.family == MissionFamily::Unknown ||
      definition.asset_ids.empty()) return false;
  for (std::size_t i = 0; i < definition.asset_ids.size(); ++i) {
    if (definition.asset_ids[i] == 0) return false;
    if (std::find(definition.asset_ids.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                  definition.asset_ids.end(), definition.asset_ids[i]) != definition.asset_ids.end()) {
      return false;
    }
  }
  return missions_.emplace(definition.id, std::move(definition)).second;
}

bool MissionCatalog::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionCatalog loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    if (first == std::string::npos || second == std::string::npos ||
        line.find('\t', second + 1) != std::string::npos) {
      return false;
    }
    std::uint32_t mission_id{};
    const auto id_text = std::string_view(line).substr(0, first);
    if (!parse_u32(id_text, mission_id)) return false;
    std::vector<AssetId> asset_ids;
    const auto assets_text = std::string_view(line).substr(second + 1);
    if (!parse_asset_ids(assets_text, asset_ids)) return false;
    if (!loaded.add({mission_id, parse_family(std::string_view(line).substr(first + 1, second - first - 1)),
                     std::move(asset_ids)})) {
      return false;
    }
  }
  missions_ = std::move(loaded.missions_);
  return true;
}

const MissionDefinition* MissionCatalog::find(std::uint32_t id) const noexcept {
  const auto it = missions_.find(id);
  return it == missions_.end() ? nullptr : &it->second;
}

MissionScenario::MissionScenario(const MissionDefinition& definition) : mission_id_(definition.id) {}

bool MissionScenario::bind_player(const UnitRegistry& units, EntityId entity) noexcept {
  const UnitRecord* unit = units.find(entity);
  if (unit == nullptr || !unit->active) return false;
  player_ = entity;
  return true;
}

bool MissionScenario::dispatch(Event event) noexcept {
  switch (event.type) {
    case EventType::StartMission:
      if (state_ != ScenarioState::Loading && state_ != ScenarioState::Briefing) return false;
      if (player_ != 0 && event.subject != player_) return false;
      state_ = ScenarioState::Gameplay;
      return true;
    case EventType::Pause:
      if (state_ != ScenarioState::Gameplay) return false;
      state_ = ScenarioState::Paused;
      return true;
    case EventType::Resume:
      if (state_ != ScenarioState::Paused) return false;
      state_ = ScenarioState::Gameplay;
      return true;
    case EventType::Complete:
      if (state_ != ScenarioState::Gameplay && state_ != ScenarioState::Paused) return false;
      if (!objectives_.all_required_complete()) return false;
      state_ = ScenarioState::Complete;
      return true;
    case EventType::Abort:
      if (state_ == ScenarioState::Complete || state_ == ScenarioState::Aborted) return false;
      state_ = ScenarioState::Aborted;
      return true;
  }
  return false;
}

bool MissionScenario::complete_objective(std::uint32_t id) noexcept {
  return objectives_.complete(id);
}

bool MissionScenario::add_objective(ObjectiveRecord objective) {
  return objectives_.add(std::move(objective));
}

bool MissionScenario::activate_objective(std::uint32_t id) noexcept {
  return objectives_.activate(id);
}

bool MissionScenario::fail_objective(std::uint32_t id) noexcept {
  if (!objectives_.fail(id)) return false;
  state_ = ScenarioState::Aborted;
  return true;
}

bool MissionScenario::evaluate_combat(const UnitRegistry& units,
                                      const CombatWorld& combat) noexcept {
  if (state_ != ScenarioState::Gameplay || player_ == 0) return false;
  bool changed = false;
  for (const ObjectiveRecord& objective : objectives_.snapshot()) {
    if (objective.state != ObjectiveState::Active ||
        objective.condition == ObjectiveCondition::Manual) {
      continue;
    }
    // A condition never completes or fails from a stale entity id.  The unit
    // registry is the authoritative scenario ownership table; combat state
    // alone is not enough to qualify a retail target binding.
    if (units.find(objective.target_entity) == nullptr) continue;
    const CombatUnitState* target = combat.unit(objective.target_entity);
    const bool target_active = target != nullptr && target->active && target->health > 0.0f;
    if (objective.condition == ObjectiveCondition::DestroyUnit && !target_active) {
      changed = objectives_.complete(objective.id) || changed;
    } else if (objective.condition == ObjectiveCondition::ProtectUnit && !target_active) {
      changed = objectives_.fail(objective.id) || changed;
    }
  }
  if (objectives_.failed_count() != 0) state_ = ScenarioState::Aborted;
  return changed;
}

bool MissionScenario::dispatch_radio(const RadioMessageDatabase& messages,
                                     std::uint32_t id) noexcept {
  if (state_ != ScenarioState::Gameplay && state_ != ScenarioState::Paused) return false;
  if (messages.find(mission_id_, id) == nullptr) return false;
  radio_history_.push_back(id);
  return true;
}

std::optional<std::uint32_t> MissionScenario::objective_index(std::uint32_t id) const noexcept {
  const std::vector<ObjectiveRecord> records = objectives_.snapshot();
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (records[index].id == id) return static_cast<std::uint32_t>(index);
  }
  return std::nullopt;
}

MissionDebrief MissionScenario::debrief() const {
  MissionDebrief result;
  result.mission_id = mission_id_;
  result.outcome = state_ == ScenarioState::Complete
                       ? MissionOutcome::Success
                       : (state_ == ScenarioState::Aborted ? MissionOutcome::Failure
                                                            : MissionOutcome::InProgress);
  result.objective_count = static_cast<std::uint32_t>(objectives_.size());
  result.completed_objectives = static_cast<std::uint32_t>(objectives_.completed_count());
  result.failed_objectives = static_cast<std::uint32_t>(objectives_.failed_count());
  result.radio_history = radio_history_;
  return result;
}

MissionScenarioSnapshot MissionScenario::snapshot() const {
  return {mission_id_, state_, player_, objectives_.snapshot(), radio_history_};
}

bool MissionScenario::restore(const MissionScenarioSnapshot& snapshot) noexcept {
  if (snapshot.mission_id != mission_id_ ||
      static_cast<std::uint8_t>(snapshot.state) >
          static_cast<std::uint8_t>(ScenarioState::Aborted) ||
      snapshot.radio_history.size() > 65536) return false;
  for (const std::uint32_t message : snapshot.radio_history) {
    if (message == 0) return false;
  }
  ObjectiveRegistry loaded;
  if (!loaded.restore(snapshot.objectives)) return false;
  objectives_ = std::move(loaded);
  state_ = snapshot.state;
  player_ = snapshot.player;
  radio_history_ = snapshot.radio_history;
  return true;
}

bool MissionScenario::dispatch_buttons(const InputMappingDatabase& mappings,
                                       std::uint16_t buttons, EntityId subject) noexcept {
  const InputBinding* binding = mappings.resolve(buttons);
  return binding != nullptr && dispatch({binding->event, subject});
}


MissionRuntime::MissionRuntime(std::uint32_t mission_id, const MissionAssetDatabase* assets)
    : mission_id_(mission_id), assets_(assets) {}

MissionRuntime::MissionRuntime(const MissionDefinition& definition,
                               const MissionAssetDatabase* assets)
    : mission_id_(definition.id), assets_(assets), definition_(&definition) {}

RuntimeSnapshot MissionRuntime::snapshot() const noexcept {
  return {tick_, position_x_, position_y_, position_z_, pitch_, roll_, yaw_, fixed_accumulator_};
}

bool MissionRuntime::restore(RuntimeSnapshot snapshot) noexcept {
  if (snapshot.tick == 0 || !std::isfinite(snapshot.position_x) ||
      !std::isfinite(snapshot.position_y) || !std::isfinite(snapshot.position_z) ||
      !std::isfinite(snapshot.pitch) || !std::isfinite(snapshot.roll) ||
      !std::isfinite(snapshot.yaw) || !std::isfinite(snapshot.fixed_accumulator) ||
      snapshot.fixed_accumulator < 0.0f || snapshot.fixed_accumulator >= 1.0f / 60.0f) return false;
  tick_ = snapshot.tick;
  position_x_ = snapshot.position_x;
  position_y_ = snapshot.position_y;
  position_z_ = snapshot.position_z;
  pitch_ = snapshot.pitch;
  roll_ = snapshot.roll;
  yaw_ = snapshot.yaw;
  fixed_accumulator_ = snapshot.fixed_accumulator;
  return true;
}

bool MissionRuntime::set_definition(const MissionDefinition* definition) noexcept {
  if (definition == nullptr || definition->id != mission_id_ ||
      definition->family == MissionFamily::Unknown || definition->asset_ids.empty()) {
    definition_ = nullptr;
    return false;
  }
  definition_ = definition;
  return true;
}

WorldFrame MissionRuntime::tick(float fixed_dt, InputFrame input) {
  const bool scheduler_stopped = scenario_ != nullptr &&
      (scenario_->state() == ScenarioState::Paused ||
       scenario_->state() == ScenarioState::Complete ||
       scenario_->state() == ScenarioState::Aborted);
  if (!scheduler_stopped) {
    if (!(fixed_dt > 0.0f) || fixed_dt > 0.25f) fixed_dt = 1.0f / 60.0f;
    constexpr float simulation_dt = 1.0f / 60.0f;
    constexpr std::uint32_t max_steps_per_call = 16;
    fixed_accumulator_ = std::min(fixed_accumulator_ + fixed_dt, 0.25f);
    const auto axis = [](std::int16_t value) {
      return std::clamp(static_cast<float>(value) / 32767.0f, -1.0f, 1.0f);
    };
    std::uint32_t steps = 0;
    while (fixed_accumulator_ + 1.0e-7f >= simulation_dt && steps < max_steps_per_call) {
      fixed_accumulator_ = std::max(0.0f, fixed_accumulator_ - simulation_dt);
      ++tick_;
      pitch_ += axis(input.pitch) * simulation_dt;
      roll_ += axis(input.roll) * simulation_dt;
      yaw_ += axis(input.yaw) * simulation_dt;
      position_x_ += yaw_ * simulation_dt;
      position_y_ += pitch_ * simulation_dt;
      position_z_ += (static_cast<float>(input.throttle) / 255.0f) * simulation_dt;
      ++steps;
    }
  }
  bool ready = assets_ != nullptr && definition_ != nullptr && scenario_ != nullptr &&
               scenario_->state() == ScenarioState::Gameplay;
  if (ready) {
    for (const AssetId id : definition_->asset_ids) {
      ready = assets_->resolve(id) != nullptr;
      if (!ready) break;
    }
  }
  const auto active_units = units_ ? static_cast<std::uint32_t>(units_->active_count()) : 0u;
  const auto player = scenario_ ? scenario_->player() : EntityId{};
  constexpr float follow_distance = 12.0f;
  constexpr float follow_height = 3.0f;
  const float forward_speed = static_cast<float>(input.throttle) / 255.0f;
  const float speed = std::sqrt(pitch_ * pitch_ + roll_ * roll_ + yaw_ * yaw_ +
                                forward_speed * forward_speed);
  return WorldFrame{tick_, mission_id_, ready, position_x_, position_y_, position_z_, pitch_, roll_, yaw_,
                    speed, active_units, player, position_x_ - follow_distance, position_y_ + follow_height,
                    position_z_ + follow_distance, position_x_, position_y_, position_z_, input};
}

MissionExecution::MissionExecution(const MissionDefinition& definition,
                                   const MissionAssetDatabase* assets,
                                   const MissionObjectiveDatabase* objectives,
                                   const RadioMessageDatabase* radios,
                                   CampaignProgression* campaign,
                                   MissionWaveDirector* waves,
                                   MissionSequenceDirector* sequence,
    const InputMappingDatabase* input,
    MissionAiDirector* ai)
    : definition_(&definition), assets_(assets), objectives_(objectives), radios_(radios), campaign_(campaign),
      waves_(waves), sequence_(sequence), input_(input), ai_(ai),
      runtime_(definition, assets),
      scenario_(definition) {}

bool MissionExecution::launch(const MissionLaunchDefinition& launch) noexcept {
  if (definition_ == nullptr || launch.mission_id != definition_->id) return false;
  if (campaign_ == nullptr) {
    // The standalone runtime path remains available for developer fixtures.
  } else {
    const CampaignMissionStatus* status = campaign_->status(definition_->id);
    if (status == nullptr || status->state != CampaignMissionState::Active) return false;
  }
  units_ = UnitRegistry{};
  combat_.clear();
  radio_.reset();
  primary_weapon_id_ = 0;
  weapon_count_ = 0;
  if (waves_ != nullptr) waves_->reset();
  if (sequence_ != nullptr) sequence_->reset();
  scenario_ = MissionScenario(*definition_);
  if (objectives_ != nullptr) {
    for (const ObjectiveRecord* objective : objectives_->find_by_mission(definition_->id)) {
      if (objective == nullptr || !scenario_.add_objective(*objective)) {
        units_ = UnitRegistry{};
        scenario_ = MissionScenario(*definition_);
        launched_ = false;
        return false;
      }
    }
  }
  if (!configure_mission_launch(launch, units_, scenario_) ||
      !scenario_.dispatch({EventType::StartMission, launch.player_entity})) {
    units_ = UnitRegistry{};
    combat_.clear();
    scenario_ = MissionScenario(*definition_);
    launched_ = false;
    return false;
  }
  std::size_t spawn_index = 0;
  for (const UnitRecord& unit : launch.units) {
    const float spawn_x = unit.id == launch.player_entity
        ? 0.0f
        : 20.0f + static_cast<float>(spawn_index++) * 5.0f;
    if (!combat_.add_unit({unit.id, unit.owner, {spawn_x, 0.0f, 0.0f},
                           100.0f, 100.0f, 1.0f, true})) {
      units_ = UnitRegistry{};
      combat_.clear();
      scenario_ = MissionScenario(*definition_);
      launched_ = false;
      return false;
    }
  }
  for (const WeaponDefinition weapon : launch.weapons) {
    if (!combat_.add_weapon(weapon)) {
      units_ = UnitRegistry{};
      combat_.clear();
      scenario_ = MissionScenario(*definition_);
      launched_ = false;
      return false;
    }
  }
  if (!launch.weapons.empty()) primary_weapon_id_ = launch.weapons.front().id;
  weapon_count_ = static_cast<std::uint32_t>(launch.weapons.size());
  runtime_.set_definition(definition_);
  runtime_.set_units(&units_);
  runtime_.set_scenario(&scenario_);
  launched_ = true;
  return true;
}

bool MissionExecution::dispatch(Event event) noexcept {
  if (!launched_) return false;
  if (event.type == EventType::Complete && campaign_ != nullptr &&
      !campaign_->can_complete(definition_->id)) return false;
  if (event.type == EventType::Abort && campaign_ != nullptr) {
    const CampaignMissionStatus* status = campaign_->status(definition_->id);
    if (status == nullptr || status->state != CampaignMissionState::Active) return false;
  }
  if (!scenario_.dispatch(event)) return false;
  if (event.type == EventType::Complete && campaign_ != nullptr) {
    return campaign_->complete(definition_->id);
  }
  if (event.type == EventType::Abort && campaign_ != nullptr) {
    return campaign_->fail(definition_->id);
  }
  return true;
}

bool MissionExecution::activate_objective(std::uint32_t id) noexcept {
  return launched_ && scenario_.activate_objective(id);
}

bool MissionExecution::complete_objective(std::uint32_t id) noexcept {
  if (!launched_) return false;
  const auto index = scenario_.objective_index(id);
  if (!index || (campaign_ != nullptr &&
                 !campaign_->can_complete_objective(definition_->id, *index))) return false;
  if (!scenario_.complete_objective(id)) return false;
  return campaign_ == nullptr || campaign_->complete_objective(definition_->id, *index);
}

bool MissionExecution::fail_objective(std::uint32_t id) noexcept {
  if (!launched_ || !scenario_.fail_objective(id)) return false;
  return campaign_ == nullptr || campaign_->fail(definition_->id);
}

bool MissionExecution::dispatch_radio(std::uint32_t id) noexcept {
  return launched_ && radios_ != nullptr && scenario_.dispatch_radio(*radios_, id);
}

bool MissionExecution::play_radio(std::uint32_t id, float duration_seconds) noexcept {
  if (!launched_ || radios_ == nullptr || !radio_.start(*radios_, definition_->id, id,
                                                          duration_seconds)) return false;
  if (scenario_.dispatch_radio(*radios_, id)) return true;
  radio_.reset();
  return false;
}

bool MissionSequenceDirector::dispatch_due(std::uint32_t mission_id, std::uint64_t tick,
                                           MissionExecution& execution) noexcept {
  for (Entry& entry : entries_) {
    if (entry.published || entry.event.mission_id != mission_id || entry.event.tick > tick) {
      continue;
    }
    bool dispatched = false;
    switch (entry.event.type) {
      case MissionSequenceEventType::ActivateObjective:
        dispatched = execution.activate_objective(entry.event.id);
        break;
      case MissionSequenceEventType::CompleteObjective:
        dispatched = execution.complete_objective(entry.event.id);
        break;
      case MissionSequenceEventType::FailObjective:
        dispatched = execution.fail_objective(entry.event.id);
        break;
      case MissionSequenceEventType::PlayRadio:
        dispatched = execution.play_radio(entry.event.id, entry.event.duration_seconds);
        break;
    }
    if (!dispatched) return false;
    entry.published = true;
  }
  return true;
}

std::size_t MissionSequenceDirector::pending(std::uint32_t mission_id) const noexcept {
  return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
      [mission_id](const Entry& entry) {
        return entry.event.mission_id == mission_id && !entry.published;
      }));
}

std::size_t MissionSequenceDirector::dispatched(std::uint32_t mission_id) const noexcept {
  return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
      [mission_id](const Entry& entry) {
        return entry.event.mission_id == mission_id && entry.published;
      }));
}

MissionSequenceSnapshot MissionSequenceDirector::snapshot() const {
  MissionSequenceSnapshot result;
  result.entries.reserve(entries_.size());
  for (const Entry& entry : entries_) result.entries.push_back({entry.event, entry.published});
  return result;
}

bool MissionSequenceDirector::restore(const MissionSequenceSnapshot& snapshot) noexcept {
  if (snapshot.entries.size() > 4096) return false;
  std::vector<Entry> loaded;
  loaded.reserve(snapshot.entries.size());
  std::uint32_t previous_mission = 0;
  std::uint64_t previous_tick = 0;
  std::uint32_t previous_order = 0;
  for (const MissionSequenceEntrySnapshot& candidate : snapshot.entries) {
    const MissionSequenceEvent& event = candidate.event;
    if (!event.valid() ||
        (event.mission_id < previous_mission) ||
        (event.mission_id == previous_mission && event.tick < previous_tick) ||
        (event.mission_id == previous_mission && event.tick == previous_tick &&
         event.order <= previous_order)) return false;
    loaded.push_back({event, candidate.published});
    previous_mission = event.mission_id;
    previous_tick = event.tick;
    previous_order = event.order;
  }
  entries_ = std::move(loaded);
  return true;
}

void MissionSequenceDirector::reset() noexcept {
  for (Entry& entry : entries_) entry.published = false;
}

bool MissionExecution::lock_target(EntityId target) noexcept {
  return launched_ && combat_.lock_target(scenario_.player(), target);
}

bool MissionExecution::fire_weapon(std::uint32_t weapon_id) noexcept {
  return launched_ && combat_.fire(scenario_.player(), weapon_id);
}

WorldFrame MissionExecution::tick(float fixed_dt, InputFrame input) noexcept {
  if (!launched_) return {};
  if (input_ != nullptr && input.buttons != 0) {
    const InputBinding* binding = input_->resolve(input.buttons);
    if (binding != nullptr && !dispatch({binding->event, scenario_.player()})) return {};
  }
  if (scenario_.state() == ScenarioState::Gameplay) combat_.tick(fixed_dt);
  WorldFrame frame = runtime_.tick(fixed_dt, input);
  if (scenario_.state() == ScenarioState::Gameplay) (void)radio_.tick(fixed_dt);
  if (scenario_.state() == ScenarioState::Gameplay && waves_ != nullptr &&
      !waves_->spawn_due(definition_->id, frame.tick, units_, combat_)) {
    frame.mission_ready = false;
    return frame;
  }
  if (scenario_.state() == ScenarioState::Gameplay && ai_ != nullptr &&
      !ai_->dispatch_due(definition_->id, frame.tick, combat_)) {
    frame.mission_ready = false;
    return frame;
  }
  if (scenario_.state() == ScenarioState::Gameplay && sequence_ != nullptr &&
      !sequence_->dispatch_due(definition_->id, frame.tick, *this)) {
    frame.mission_ready = false;
    return frame;
  }
  for (const CombatUnitState& unit : combat_.snapshot_units()) {
    (void)units_.set_active(unit.entity, unit.active && unit.health > 0.0f);
  }
  frame.active_units = static_cast<std::uint32_t>(units_.active_count());
  (void)scenario_.evaluate_combat(units_, combat_);
  if (scenario_.state() != ScenarioState::Gameplay) frame.mission_ready = false;
  if (scenario_.state() == ScenarioState::Gameplay) {
    const CombatUnitState* player = combat_.unit(scenario_.player());
    const bool player_destroyed = player == nullptr || !player->active;
    const bool expired = failure_tick_ != 0 && frame.tick >= failure_tick_;
    if (player_destroyed || expired) {
      if (dispatch({EventType::Abort, scenario_.player()})) {
        frame.mission_ready = false;
        frame.active_units = static_cast<std::uint32_t>(combat_.active_units());
      }
    }
  }
  if (scenario_.state() == ScenarioState::Gameplay && scenario_.objectives().size() != 0 &&
      scenario_.objectives().all_required_complete()) {
    // Objective completion is the native HSM terminal condition. Keep the
    // explicit dispatch API for qualified event consumers, but do not require
    // an external caller to synthesize the mission-complete event.
    if (dispatch({EventType::Complete, scenario_.player()})) frame.mission_ready = false;
  }
  return frame;
}

WorldFrame MissionExecution::run_replay(float fixed_dt, const ReplayLog& replay) noexcept {
  WorldFrame frame{};
  for (const InputFrame input : replay.frames()) frame = tick(fixed_dt, input);
  return frame;
}

RuntimeSnapshot MissionExecution::snapshot() const noexcept {
  return runtime_.snapshot();
}

MissionDebrief MissionExecution::debrief() const {
  return scenario_.debrief();
}

bool MissionExecution::restore(RuntimeSnapshot snapshot) noexcept {
  return launched_ && runtime_.restore(snapshot);
}

bool MissionExecution::save_checkpoint(Checkpoint& checkpoint) const noexcept {
  if (!launched_ || runtime_.snapshot().tick == 0 || combat_.active_projectiles() != 0) {
    return false;
  }
  Checkpoint candidate;
  candidate.mission_id = definition_ == nullptr ? 0 : definition_->id;
  candidate.flight = runtime_.snapshot();
  candidate.scenario = scenario_.snapshot();
  candidate.unit_records = units_.snapshot();
  candidate.combat_units = combat_.snapshot_units();
  if (assets_ != nullptr) {
    candidate.resource_identities.reserve(definition_->asset_ids.size());
    for (const AssetId id : definition_->asset_ids) {
      const AssetRecord* resource = assets_->resolve(id);
      if (resource == nullptr || !resource->valid()) return false;
      candidate.resource_identities.push_back(*resource);
    }
    std::sort(candidate.resource_identities.begin(), candidate.resource_identities.end(),
              [](const AssetRecord& left, const AssetRecord& right) {
                return left.id < right.id;
              });
  }
  candidate.failure_tick = failure_tick_;
  candidate.waves = waves_ == nullptr ? MissionWaveSnapshot{} : waves_->snapshot();
  candidate.sequence = sequence_ == nullptr ? MissionSequenceSnapshot{} : sequence_->snapshot();
  candidate.radio_playback = radio_.snapshot();
  if (candidate.mission_id == 0) return false;
  checkpoint = std::move(candidate);
  return true;
}

bool MissionExecution::restore_checkpoint(const Checkpoint& checkpoint) noexcept {
  if (!launched_ || definition_ == nullptr || checkpoint.mission_id != definition_->id ||
      checkpoint.scenario.mission_id != definition_->id ||
      checkpoint.scenario.player == 0 ||
      checkpoint.combat_units.empty() ||
      checkpoint.resource_identities.size() > 4096 ||
      (sequence_ == nullptr && !checkpoint.sequence.entries.empty()) ||
      (waves_ == nullptr && !checkpoint.waves.entries.empty())) return false;
  if (std::find_if(checkpoint.combat_units.begin(), checkpoint.combat_units.end(),
                   [&](const CombatUnitState& unit) {
                     return unit.entity == checkpoint.scenario.player;
                   }) == checkpoint.combat_units.end()) return false;
  AssetId previous_resource = 0;
  for (const AssetRecord& resource : checkpoint.resource_identities) {
    if (!resource.valid() || resource.id <= previous_resource) return false;
    previous_resource = resource.id;
  }
  if (!checkpoint.resource_identities.empty()) {
    if (assets_ == nullptr || checkpoint.resource_identities.size() != definition_->asset_ids.size()) {
      return false;
    }
    for (const AssetRecord& checkpoint_resource : checkpoint.resource_identities) {
      const AssetRecord* current_resource = assets_->resolve(checkpoint_resource.id);
      if (current_resource == nullptr || *current_resource != checkpoint_resource) return false;
    }
  }
  for (const MissionSequenceEntrySnapshot& entry : checkpoint.sequence.entries) {
    if (!entry.event.valid() || entry.event.mission_id != definition_->id) return false;
  }
  if (!checkpoint.unit_records.empty()) {
    EntityId previous_unit_record = 0;
    for (const UnitRecord& record : checkpoint.unit_records) {
      if (record.id == 0 || record.asset == 0 || record.owner == record.id ||
          record.id <= previous_unit_record) return false;
      previous_unit_record = record.id;
      const auto combat_unit = std::find_if(
          checkpoint.combat_units.begin(), checkpoint.combat_units.end(),
          [record](const CombatUnitState& unit) { return unit.entity == record.id; });
      if (combat_unit == checkpoint.combat_units.end() || combat_unit->faction != record.owner) {
        return false;
      }
    }
    for (const CombatUnitState& unit : checkpoint.combat_units) {
      if (std::find_if(checkpoint.unit_records.begin(), checkpoint.unit_records.end(),
                       [unit](const UnitRecord& record) { return record.id == unit.entity; }) ==
          checkpoint.unit_records.end()) return false;
    }
  }
  if (waves_ != nullptr && !checkpoint.waves.entries.empty()) {
    MissionWaveDirector validated_waves;
    if (!validated_waves.restore(checkpoint.waves)) return false;
  }
  const RuntimeSnapshot old_flight = runtime_.snapshot();
  const MissionScenarioSnapshot old_scenario = scenario_.snapshot();
  const std::vector<UnitRecord> old_unit_records = units_.snapshot();
  const std::vector<CombatUnitState> old_units = combat_.snapshot_units();
  const std::uint64_t old_failure_tick = failure_tick_;
  const MissionWaveSnapshot old_waves = waves_ == nullptr ? MissionWaveSnapshot{} : waves_->snapshot();
  const MissionSequenceSnapshot old_sequence =
      sequence_ == nullptr ? MissionSequenceSnapshot{} : sequence_->snapshot();
  const RadioPlaybackSnapshot old_radio = radio_.snapshot();
  if (!runtime_.restore(checkpoint.flight) || !scenario_.restore(checkpoint.scenario) ||
      (!checkpoint.unit_records.empty() && !units_.restore(checkpoint.unit_records)) ||
      !combat_.restore_units(checkpoint.combat_units) ||
      (waves_ != nullptr && !checkpoint.waves.entries.empty() &&
       !waves_->restore(checkpoint.waves)) ||
      (sequence_ != nullptr && !sequence_->restore(checkpoint.sequence)) ||
      !radio_.restore(checkpoint.radio_playback)) {
    (void)runtime_.restore(old_flight);
    (void)scenario_.restore(old_scenario);
    (void)units_.restore(old_unit_records);
    (void)combat_.restore_units(old_units);
    failure_tick_ = old_failure_tick;
    if (waves_ != nullptr) (void)waves_->restore(old_waves);
    if (sequence_ != nullptr) (void)sequence_->restore(old_sequence);
    (void)radio_.restore(old_radio);
    return false;
  }
  failure_tick_ = checkpoint.failure_tick;
  return true;
}

WorldFrame MissionRuntime::run_replay(float fixed_dt, const ReplayLog& replay) {
  WorldFrame frame{};
  for (const InputFrame input : replay.frames()) frame = tick(fixed_dt, input);
  return frame;
}

}  // namespace ac6
