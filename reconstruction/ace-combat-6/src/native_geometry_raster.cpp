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
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
         std::isfinite(value);
}

bool parse_bool01(std::string_view text, bool& value) noexcept {
  if (text == "0") {
    value = false;
    return true;
  }
  if (text == "1") {
    value = true;
    return true;
  }
  return false;
}

bool parse_objective_condition(std::string_view text,
                               ObjectiveCondition& condition) noexcept {
  if (text == "manual") {
    condition = ObjectiveCondition::Manual;
    return true;
  }
  if (text == "destroy_unit") {
    condition = ObjectiveCondition::DestroyUnit;
    return true;
  }
  if (text == "protect_unit") {
    condition = ObjectiveCondition::ProtectUnit;
    return true;
  }
  return false;
}

bool parse_hex_u32(std::string_view text, std::uint32_t& value) noexcept {
  if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    text.remove_prefix(2);
  }
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);
  return !text.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parse_hex_u64(std::string_view text, std::uint64_t& value) noexcept {
  if (text.size() >= 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    text.remove_prefix(2);
  }
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 16);
  return !text.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
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

}  // namespace
bool NativeGeometryDatabase::load_verified_binary(
    const MissionDrawable& drawable, const std::vector<unsigned char>& raw) {
        const auto be16 = [&raw](std::size_t offset, std::uint16_t& value) {
          if (offset > raw.size() || raw.size() - offset < sizeof(value)) return false;
          value = static_cast<std::uint16_t>(static_cast<std::uint16_t>(raw[offset]) << 8u) |
                  static_cast<std::uint16_t>(raw[offset + 1u]);
          return true;
        };
        const auto be32 = [&raw](std::size_t offset, std::uint32_t& value) {
          if (offset > raw.size() || raw.size() - offset < sizeof(value)) return false;
          value = (static_cast<std::uint32_t>(raw[offset]) << 24u) |
                  (static_cast<std::uint32_t>(raw[offset + 1u]) << 16u) |
                  (static_cast<std::uint32_t>(raw[offset + 2u]) << 8u) |
                  static_cast<std::uint32_t>(raw[offset + 3u]);
          return true;
        };
        const auto bef32 = [&raw, &be32](std::size_t offset, float& value) {
          std::uint32_t bits = 0;
          if (!be32(offset, bits)) return false;
          std::memcpy(&value, &bits, sizeof(value));
          return std::isfinite(value);
        };
        std::uint32_t declared_size = 0, header_size = 0, polygon_size = 0;
        std::uint32_t vertex_size = 0, additional_size = 0;
        std::uint16_t object_count = 0;
        if (!be32(4, declared_size) || declared_size != raw.size() || !be16(0x0a, object_count) ||
            object_count == 0 || object_count > 100000 || !be32(0x10, header_size) ||
            !be32(0x14, polygon_size) || !be32(0x18, vertex_size) ||
            !be32(0x1c, additional_size)) return false;
        const std::size_t object_table = 0x30u;
        const std::size_t polygon_descriptors = object_table +
            static_cast<std::size_t>(object_count) * 0x30u;
        if (polygon_descriptors > raw.size() || header_size > raw.size() - 0x30u) return false;
        std::uint32_t polygon_count = 0;
        for (std::uint32_t object = 0; object < object_count; ++object) {
          std::uint16_t count = 0;
          if (!be16(object_table + static_cast<std::size_t>(object) * 0x30u + 0x2au, count) ||
              polygon_count > 100000u - count) return false;
          polygon_count += count;
        }
        const std::size_t polygon_descriptor_end = polygon_descriptors +
            static_cast<std::size_t>(polygon_count) * 0x30u;
        const std::size_t polygon_base = 0x30u + static_cast<std::size_t>(header_size);
        if (polygon_descriptor_end > polygon_base || polygon_base > raw.size() ||
            polygon_size > raw.size() - polygon_base) return false;
        const std::size_t vertex_base = polygon_base + polygon_size;
        if (vertex_base > raw.size() || vertex_size > raw.size() - vertex_base ||
            additional_size > raw.size() - vertex_base - vertex_size) return false;

        struct Polygon { std::uint32_t index_offset{}, vertex_offset{}; std::uint16_t vertex_count{}, index_count{}, format{}; };
        std::vector<Polygon> polygons;
        polygons.reserve(polygon_count);
        std::uint32_t vertex_stride = 0;
        std::uint64_t max_vertex_end = 0;
        std::uint64_t total_indices = 0;
        for (std::uint32_t polygon = 0; polygon < polygon_count; ++polygon) {
          const std::size_t offset = polygon_descriptors + static_cast<std::size_t>(polygon) * 0x30u;
          Polygon value;
          if (!be32(offset, value.index_offset) || !be32(offset + 4u, value.vertex_offset) ||
              !be16(offset + 0x0cu, value.vertex_count) || !be16(offset + 0x0eu, value.format) ||
              !be16(offset + 0x20u, value.index_count) ||
              value.index_offset > polygon_size ||
              static_cast<std::uint64_t>(value.index_count) * 2u > polygon_size - value.index_offset) {
            return false;
          }
          const std::uint32_t stride = value.format == 0x0611u ? 28u :
                                       value.format == 0x0613u ? 32u :
                                       value.format == 0x0711u ? 44u :
                                       value.format == 0x0721u ? 52u : 0u;
          if (stride == 0 || (vertex_stride != 0 && vertex_stride != stride)) return false;
          vertex_stride = stride;
          if (value.vertex_offset % stride != 0 || value.vertex_offset > vertex_size ||
              static_cast<std::uint64_t>(value.vertex_count) * stride > vertex_size - value.vertex_offset ||
              total_indices > std::numeric_limits<std::uint32_t>::max() - value.index_count) return false;
          total_indices += value.index_count;
          max_vertex_end = std::max(max_vertex_end,
              static_cast<std::uint64_t>(value.vertex_offset) +
              static_cast<std::uint64_t>(value.vertex_count) * stride);
          polygons.push_back(value);
        }
        if (vertex_stride == 0 || max_vertex_end == 0 ||
            (max_vertex_end + vertex_stride - 1u) / vertex_stride > std::numeric_limits<std::uint32_t>::max() ||
            total_indices > std::numeric_limits<std::uint32_t>::max() ||
            drawable.vertex_count != vertex_size / vertex_stride ||
            drawable.index_count != total_indices || drawable.primitive_count != polygon_count) {
          return false;
        }
        NativeGeometryMetadata metadata;
        metadata.buffer_id = drawable.buffer_id;
        metadata.source_format = "NDXR_BE";
        metadata.vertex_count = static_cast<std::uint32_t>((max_vertex_end + vertex_stride - 1u) / vertex_stride);
        metadata.index_count = static_cast<std::uint32_t>(total_indices);
        metadata.primitive_count = polygon_count;
        metadata.vertex_section_count = metadata.vertex_count;
        metadata.index_section_count = metadata.index_count;
        metadata.polygon_descriptor_count = polygon_count;
        metadata.vertex_stride = vertex_stride;
        metadata.index_size = 2;
        metadata.vertex_byte_size = vertex_size;
        metadata.index_byte_size = total_indices * 2u;
        DecodedGeometry decoded;
        bool has_primitive_restart = false;
        decoded.buffer_id = drawable.buffer_id;
        decoded.vertices.reserve(metadata.vertex_count);
        for (std::uint32_t vertex = 0; vertex < metadata.vertex_count; ++vertex) {
          const std::size_t offset = vertex_base + static_cast<std::size_t>(vertex) * vertex_stride;
          DecodedVertex value;
          if (!bef32(offset, value.x) || !bef32(offset + 4u, value.y) ||
              !bef32(offset + 8u, value.z)) return false;
          const std::size_t uv_offset = vertex_stride == 28u ? 16u : 20u;
          if (vertex_stride >= 28u) {
            std::uint32_t u_bits = 0, v_bits = 0;
            if (!be32(offset + uv_offset, u_bits) || !be32(offset + uv_offset + 4u, v_bits)) return false;
            std::memcpy(&value.u, &u_bits, sizeof(value.u));
            std::memcpy(&value.v, &v_bits, sizeof(value.v));
          }
          if (!std::isfinite(value.u) || !std::isfinite(value.v)) value.u = value.v = 0.0f;
          if (!decoded.bounds.valid) decoded.bounds = {value.x, value.y, value.z, value.x, value.y, value.z, true};
          else {
            decoded.bounds.min_x = std::min(decoded.bounds.min_x, value.x);
            decoded.bounds.min_y = std::min(decoded.bounds.min_y, value.y);
            decoded.bounds.min_z = std::min(decoded.bounds.min_z, value.z);
            decoded.bounds.max_x = std::max(decoded.bounds.max_x, value.x);
            decoded.bounds.max_y = std::max(decoded.bounds.max_y, value.y);
            decoded.bounds.max_z = std::max(decoded.bounds.max_z, value.z);
          }
          decoded.vertices.push_back(value);
        }
        decoded.indices.reserve(static_cast<std::size_t>(total_indices) + polygons.size());
        for (std::size_t polygon_index = 0; polygon_index < polygons.size(); ++polygon_index) {
          const Polygon& polygon = polygons[polygon_index];
          // Each retail NDXR polygon is an independent triangle strip.  The
          // container stores polygon boundaries in descriptors rather than
          // injecting restart indices, so preserve that ownership explicitly
          // in the decoded stream before concatenating polygons.
          if (polygon_index != 0) {
            decoded.indices.push_back(std::numeric_limits<std::uint32_t>::max());
            has_primitive_restart = true;
          }
          const std::uint32_t vertex_base_index = polygon.vertex_offset / vertex_stride;
          for (std::uint32_t index = 0; index < polygon.index_count; ++index) {
            std::uint16_t local = 0;
            if (!be16(polygon_base + polygon.index_offset + static_cast<std::size_t>(index) * 2u, local)) return false;
            if (local == 0xffffu) {
              // Preserve the restart boundary: retail NDXR polygons are
              // triangle strips, not a flat triangle-list stream.
              has_primitive_restart = true;
              decoded.indices.push_back(std::numeric_limits<std::uint32_t>::max());
              continue;
            }
            if (local >= polygon.vertex_count || vertex_base_index + local >= metadata.vertex_count) return false;
            decoded.indices.push_back(vertex_base_index + local);
          }
        }
        if (!decoded.bounds.valid || decoded.indices.empty()) return false;
        metadata.topology = NativeIndexTopology::TriangleStripRestart;
        geometries_.push_back(std::move(metadata));
        decoded_.push_back(std::move(decoded));
        return true;
}

bool NativeGeometryDatabase::load_verified_text(const MissionDrawable& drawable,
                                                 const QualifiedBufferRecord& record,
                                                 std::ifstream& input) {
  input.clear();
  input.seekg(0, std::ios::beg);
  if (!input) return false;
  std::string line;
  if (!std::getline(input, line)) return false;
  const auto first = line.find('\t');
  const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
  const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
  const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
  if (first == std::string::npos || second == std::string::npos ||
      third == std::string::npos || fourth == std::string::npos ||
      line.find('\t', fourth + 1) != std::string::npos ||
      line.substr(0, first) != "NDXR" ||
      line.substr(first + 1, second - first - 1) != "1") {
    return false;
  }
  NativeGeometryMetadata metadata;
  metadata.buffer_id = drawable.buffer_id;
  metadata.source_format = "NDXR";
  if (!parse_u32(std::string_view(line).substr(second + 1, third - second - 1),
                 metadata.vertex_count) ||
      !parse_u32(std::string_view(line).substr(third + 1, fourth - third - 1),
                 metadata.index_count) ||
      !parse_u32(std::string_view(line).substr(fourth + 1), metadata.primitive_count)) {
    return false;
  }
  if (metadata.vertex_count != drawable.vertex_count ||
      metadata.index_count != drawable.index_count ||
      metadata.primitive_count != drawable.primitive_count) {
    return false;
  }
  std::uint64_t payload_byte_size = 0;
  std::uint64_t payload_start_offset = 0;
  bool saw_payload = false;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    if (line == "DATA") {
      const auto payload_start = input.tellg();
      if (payload_start < 0 || record.byte_size < static_cast<std::uint64_t>(payload_start)) {
        return false;
      }
      payload_start_offset = static_cast<std::uint64_t>(payload_start);
      payload_byte_size = record.byte_size - static_cast<std::uint64_t>(payload_start);
      saw_payload = true;
      break;
    }
    const auto section_first = line.find('\t');
    const auto section_second = section_first == std::string::npos ? std::string::npos :
        line.find('\t', section_first + 1);
    if (section_first == std::string::npos || section_second == std::string::npos ||
        line.find('\t', section_second + 1) != std::string::npos) {
      return false;
    }
    const auto section = line.substr(0, section_first);
    std::uint32_t count = 0;
    std::uint32_t stride_or_flags = 0;
    if (!parse_u32(std::string_view(line).substr(section_first + 1,
                                                 section_second - section_first - 1), count) ||
        !parse_u32(std::string_view(line).substr(section_second + 1), stride_or_flags)) {
      return false;
    }
    if (section == "VTX") {
      if (metadata.vertex_section_count != 0 || count != metadata.vertex_count ||
          stride_or_flags == 0) {
        return false;
      }
      metadata.vertex_section_count = count;
      metadata.vertex_stride = stride_or_flags;
    } else if (section == "IDX") {
      if (metadata.index_section_count != 0 || count != metadata.index_count ||
          (stride_or_flags != 2 && stride_or_flags != 4)) {
        return false;
      }
      metadata.index_section_count = count;
      metadata.index_size = stride_or_flags;
    } else if (section == "POLY") {
      if (metadata.polygon_descriptor_count != 0 || count != metadata.primitive_count) {
        return false;
      }
      metadata.polygon_descriptor_count = count;
    } else {
      return false;
    }
  }
  if (metadata.vertex_section_count != metadata.vertex_count ||
      metadata.index_section_count != metadata.index_count ||
      metadata.polygon_descriptor_count != metadata.primitive_count || !saw_payload) {
    return false;
  }
  metadata.vertex_byte_size = static_cast<std::uint64_t>(metadata.vertex_count) *
                              static_cast<std::uint64_t>(metadata.vertex_stride);
  metadata.index_byte_size = static_cast<std::uint64_t>(metadata.index_count) *
                             static_cast<std::uint64_t>(metadata.index_size);
  if (metadata.vertex_byte_size == 0 || metadata.index_byte_size == 0 ||
      metadata.vertex_byte_size + metadata.index_byte_size > payload_byte_size) {
    return false;
  }
  if (metadata.vertex_stride < 12) return false;

  DecodedGeometry decoded;
  decoded.buffer_id = drawable.buffer_id;
  // A qualified slice is the complete drawable contract, not a preview. Keep
  // one explicit allocation guard, then decode every declared vertex/index so
  // the native renderer can submit the same topology as the oracle.
  constexpr std::uint32_t max_decoded_vertices = 1'000'000;
  constexpr std::uint32_t max_decoded_indices = 4'000'000;
  if (metadata.vertex_count > max_decoded_vertices || metadata.index_count > max_decoded_indices) {
    return false;
  }
  const std::uint32_t vertex_samples = metadata.vertex_count;
  const std::uint32_t index_samples = metadata.index_count;
  decoded.vertices.reserve(vertex_samples);
  decoded.indices.reserve(index_samples);

  for (std::uint32_t i = 0; i < vertex_samples; ++i) {
    unsigned char bytes[12]{};
    const std::uint64_t offset = payload_start_offset +
        static_cast<std::uint64_t>(i) * static_cast<std::uint64_t>(metadata.vertex_stride);
    if (!read_exact_at(input, offset, bytes, sizeof(bytes))) return false;
    const DecodedVertex vertex{read_le_f32(bytes), read_le_f32(bytes + 4), read_le_f32(bytes + 8)};
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) {
      return false;
    }
    if (!decoded.bounds.valid) {
      decoded.bounds = {vertex.x, vertex.y, vertex.z, vertex.x, vertex.y, vertex.z, true};
    } else {
      decoded.bounds.min_x = std::min(decoded.bounds.min_x, vertex.x);
      decoded.bounds.min_y = std::min(decoded.bounds.min_y, vertex.y);
      decoded.bounds.min_z = std::min(decoded.bounds.min_z, vertex.z);
      decoded.bounds.max_x = std::max(decoded.bounds.max_x, vertex.x);
      decoded.bounds.max_y = std::max(decoded.bounds.max_y, vertex.y);
      decoded.bounds.max_z = std::max(decoded.bounds.max_z, vertex.z);
    }
    decoded.vertices.push_back(vertex);
  }

  const std::uint64_t index_stream_offset = payload_start_offset + metadata.vertex_byte_size;
  for (std::uint32_t i = 0; i < index_samples; ++i) {
    unsigned char bytes[4]{};
    const std::uint64_t offset = index_stream_offset +
        static_cast<std::uint64_t>(i) * static_cast<std::uint64_t>(metadata.index_size);
    if (!read_exact_at(input, offset, bytes, metadata.index_size)) return false;
    const std::uint32_t index = metadata.index_size == 2 ? read_le_u16(bytes) : read_le_u32(bytes);
    if (index >= metadata.vertex_count) return false;
    decoded.indices.push_back(index);
  }
  if (!decoded.bounds.valid) return false;

  geometries_.push_back(std::move(metadata));
  decoded_.push_back(std::move(decoded));
  return true;
}

bool NativeGeometryDatabase::load_verified(const MissionDrawable& drawable,
                                           const QualifiedBufferDatabase& buffers) {
  if (!drawable.has_buffer_contract() || !buffers.has_verified(drawable.buffer_id) ||
      find(drawable.buffer_id) != nullptr) {
    return false;
  }
  const QualifiedBufferRecord* record = buffers.find(drawable.buffer_id);
  if (record == nullptr) return false;
  std::ifstream input(record->path, std::ios::binary);
  if (!input) return false;
  // Retail NDXR slices are binary, big-endian records. The text form below is
  // retained for deterministic unit fixtures, but a qualified retail slice
  // must be decoded without rewriting its bytes into a synthetic container.
  if (record->byte_size >= 0x30 && record->byte_size <= 256u * 1024u * 1024u) {
    std::vector<unsigned char> raw(static_cast<std::size_t>(record->byte_size));
    input.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    if (input && raw.size() >= 0x30 && std::memcmp(raw.data(), "NDXR", 4) == 0 && raw[4] == 0) {
      return load_verified_binary(drawable, raw);
    }
  }
  return load_verified_text(drawable, *record, input);
}
const NativeGeometryMetadata* NativeGeometryDatabase::find(
    const std::string& buffer_id) const noexcept {
  const auto it = std::find_if(geometries_.begin(), geometries_.end(),
                               [&buffer_id](const NativeGeometryMetadata& metadata) {
                                 return metadata.buffer_id == buffer_id;
                               });
  return it == geometries_.end() ? nullptr : &*it;
}

const DecodedGeometry* NativeGeometryDatabase::decoded(
    const std::string& buffer_id) const noexcept {
  const auto it = std::find_if(decoded_.begin(), decoded_.end(),
                               [&buffer_id](const DecodedGeometry& decoded) {
                                 return decoded.buffer_id == buffer_id;
                               });
  return it == decoded_.end() ? nullptr : &*it;
}

}  // namespace ac6
