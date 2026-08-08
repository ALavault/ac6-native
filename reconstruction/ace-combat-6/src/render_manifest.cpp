#include "ac6/product_runtime.h"
#include "text_parse.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <string_view>
#include <utility>

namespace ac6 {

namespace {

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

bool parse_asset_ids(std::string_view text, std::vector<AssetId>& asset_ids) {
  while (!text.empty()) {
    const auto comma = text.find(',');
    const auto token = text.substr(0, comma);
    AssetId id{};
    if (token.empty() || !detail::parse_u32(token, id) || id == 0 ||
        std::find(asset_ids.begin(), asset_ids.end(), id) != asset_ids.end()) {
      return false;
    }
    asset_ids.push_back(id);
    if (comma == std::string_view::npos) break;
    text.remove_prefix(comma + 1);
  }
  return !asset_ids.empty();
}

}  // namespace

bool MissionRenderDatabase::add(MissionRenderDefinition definition) {
  if (definition.mission_id == 0 || definition.asset_ids.empty()) return false;
  for (std::size_t i = 0; i < definition.asset_ids.size(); ++i) {
    if (definition.asset_ids[i] == 0) return false;
    if (std::find(definition.asset_ids.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                  definition.asset_ids.end(), definition.asset_ids[i]) != definition.asset_ids.end()) {
      return false;
    }
  }
  return renders_.emplace(definition.mission_id, std::move(definition)).second;
}

bool MissionRenderDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionRenderDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    if (first == std::string::npos || line.find('\t', first + 1) != std::string::npos) {
      return false;
    }
    MissionRenderDefinition definition;
    if (!detail::parse_u32(std::string_view(line).substr(0, first), definition.mission_id) ||
        !parse_asset_ids(std::string_view(line).substr(first + 1), definition.asset_ids)) {
      return false;
    }
    if (!loaded.add(std::move(definition))) {
      return false;
    }
  }
  renders_ = std::move(loaded.renders_);
  return true;
}

const MissionRenderDefinition* MissionRenderDatabase::find(
    std::uint32_t mission_id) const noexcept {
  const auto it = renders_.find(mission_id);
  return it == renders_.end() ? nullptr : &it->second;
}

bool MissionDrawableDatabase::add(MissionDrawable drawable) {
  if (drawable.mission_id == 0 || drawable.stable_id.empty() || drawable.kind.empty() ||
      drawable.asset == 0 || drawable.primitive_count == 0 ||
      !drawable.has_buffer_contract()) {
    return false;
  }
  if (find(drawable.mission_id, drawable.stable_id) != nullptr) return false;
  drawables_.push_back(drawable);
  return true;
}

bool MissionDrawableDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionDrawableDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    const auto fifth = fourth == std::string::npos ? std::string::npos : line.find('\t', fourth + 1);
    const auto sixth = fifth == std::string::npos ? std::string::npos : line.find('\t', fifth + 1);
    const auto seventh = sixth == std::string::npos ? std::string::npos : line.find('\t', sixth + 1);
    const auto eighth = seventh == std::string::npos ? std::string::npos : line.find('\t', seventh + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        fifth == std::string::npos || sixth == std::string::npos ||
        seventh == std::string::npos || eighth == std::string::npos ||
        line.find('\t', eighth + 1) != std::string::npos) {
      return false;
    }
    MissionDrawable drawable;
    if (!detail::parse_u32(std::string_view(line).substr(0, first), drawable.mission_id) ||
        (drawable.stable_id = line.substr(first + 1, second - first - 1)).empty() ||
        (drawable.kind = line.substr(second + 1, third - second - 1)).empty() ||
        !detail::parse_u32(std::string_view(line).substr(third + 1, fourth - third - 1), drawable.asset) ||
        !detail::parse_u32(std::string_view(line).substr(fourth + 1, fifth - fourth - 1), drawable.primitive_count) ||
        (drawable.buffer_id = line.substr(fifth + 1, sixth - fifth - 1)).empty() ||
        !detail::parse_u32(std::string_view(line).substr(sixth + 1, seventh - sixth - 1), drawable.vertex_count) ||
        !detail::parse_u32(std::string_view(line).substr(seventh + 1, eighth - seventh - 1), drawable.index_count) ||
        (drawable.content_hash = line.substr(eighth + 1)).empty()) {
      return false;
    }
    if (!loaded.add(std::move(drawable))) {
      return false;
    }
  }
  drawables_ = std::move(loaded.drawables_);
  return true;
}

const MissionDrawable* MissionDrawableDatabase::find(
    std::uint32_t mission_id, const std::string& stable_id) const noexcept {
  const auto it = std::find_if(drawables_.begin(), drawables_.end(),
                               [mission_id, &stable_id](const MissionDrawable& drawable) {
                                 return drawable.mission_id == mission_id &&
                                        drawable.stable_id == stable_id;
                               });
  return it == drawables_.end() ? nullptr : &*it;
}

std::vector<const MissionDrawable*> MissionDrawableDatabase::find_by_asset(
    std::uint32_t mission_id, AssetId asset) const {
  std::vector<const MissionDrawable*> result;
  for (const MissionDrawable& drawable : drawables_) {
    if (drawable.mission_id == mission_id && drawable.asset == asset) {
      result.push_back(&drawable);
    }
  }
  return result;
}

bool MissionDrawableTransform::valid() const noexcept {
  return mission_id != 0 && !stable_id.empty() &&
         std::isfinite(translate_x) && std::isfinite(translate_y) &&
         std::isfinite(translate_z) && std::isfinite(scale_x) &&
         std::isfinite(scale_y) && std::isfinite(scale_z) &&
         scale_x > 0.0f && scale_y > 0.0f && scale_z > 0.0f;
}

bool MissionTransformDatabase::add(MissionDrawableTransform transform) {
  if (!transform.valid() || find(transform.mission_id, transform.stable_id) != nullptr) {
    return false;
  }
  transforms_.push_back(std::move(transform));
  return true;
}

bool MissionTransformDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionTransformDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    const auto fifth = fourth == std::string::npos ? std::string::npos : line.find('\t', fourth + 1);
    const auto sixth = fifth == std::string::npos ? std::string::npos : line.find('\t', fifth + 1);
    const auto seventh = sixth == std::string::npos ? std::string::npos : line.find('\t', sixth + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        fifth == std::string::npos || sixth == std::string::npos ||
        seventh == std::string::npos || line.find('\t', seventh + 1) != std::string::npos) {
      return false;
    }
    MissionDrawableTransform transform;
    if (!detail::parse_u32(std::string_view(line).substr(0, first), transform.mission_id) ||
        (transform.stable_id = line.substr(first + 1, second - first - 1)).empty() ||
        !detail::parse_f32(std::string_view(line).substr(second + 1, third - second - 1),
                   transform.translate_x) ||
        !detail::parse_f32(std::string_view(line).substr(third + 1, fourth - third - 1),
                   transform.translate_y) ||
        !detail::parse_f32(std::string_view(line).substr(fourth + 1, fifth - fourth - 1),
                   transform.translate_z) ||
        !detail::parse_f32(std::string_view(line).substr(fifth + 1, sixth - fifth - 1),
                   transform.scale_x) ||
        !detail::parse_f32(std::string_view(line).substr(sixth + 1, seventh - sixth - 1),
                   transform.scale_y) ||
        !detail::parse_f32(std::string_view(line).substr(seventh + 1), transform.scale_z)) {
      return false;
    }
    if (!loaded.add(std::move(transform))) {
      return false;
    }
  }
  transforms_ = std::move(loaded.transforms_);
  return true;
}

const MissionDrawableTransform* MissionTransformDatabase::find(
    std::uint32_t mission_id, const std::string& stable_id) const noexcept {
  const auto it = std::find_if(transforms_.begin(), transforms_.end(),
                               [mission_id, &stable_id](const MissionDrawableTransform& transform) {
                                 return transform.mission_id == mission_id &&
                                        transform.stable_id == stable_id;
                               });
  return it == transforms_.end() ? nullptr : &*it;
}

bool MissionMaterial::valid() const noexcept {
  return mission_id != 0 && !stable_id.empty() && !shader_permutation.empty() &&
         (blend_mode == "opaque" || blend_mode == "alpha" || blend_mode == "additive") &&
         ((base_color >> 24u) != 0u);
}

bool MissionMaterialDatabase::add(MissionMaterial material) {
  if (!material.valid() || find(material.mission_id, material.stable_id) != nullptr) {
    return false;
  }
  materials_.push_back(std::move(material));
  return true;
}

bool MissionMaterialDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionMaterialDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    const auto fifth = fourth == std::string::npos ? std::string::npos : line.find('\t', fourth + 1);
    const auto sixth = fifth == std::string::npos ? std::string::npos : line.find('\t', fifth + 1);
    const auto seventh = sixth == std::string::npos ? std::string::npos : line.find('\t', sixth + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        fifth == std::string::npos || sixth == std::string::npos ||
        (seventh != std::string::npos && line.find('\t', seventh + 1) != std::string::npos)) {
      return false;
    }
    MissionMaterial material;
    if (!detail::parse_u32(std::string_view(line).substr(0, first), material.mission_id) ||
        (material.stable_id = line.substr(first + 1, second - first - 1)).empty() ||
        (material.shader_permutation = line.substr(second + 1, third - second - 1)).empty() ||
        !detail::parse_bool01(std::string_view(line).substr(third + 1, fourth - third - 1),
                      material.depth_test) ||
        !detail::parse_bool01(std::string_view(line).substr(fourth + 1, fifth - fourth - 1),
                      material.depth_write) ||
        (material.blend_mode = line.substr(fifth + 1, sixth - fifth - 1)).empty() ||
        !detail::parse_hex_u32(std::string_view(line).substr(sixth + 1,
                                                      seventh == std::string::npos ?
                                                          std::string::npos : seventh - sixth - 1),
                       material.base_color) ||
        (seventh != std::string::npos &&
         (!detail::parse_u64(std::string_view(line).substr(seventh + 1), material.mate_id) ||
          material.mate_id == 0))) {
      return false;
    }
    if (!loaded.add(std::move(material))) {
      return false;
    }
  }
  materials_ = std::move(loaded.materials_);
  return true;
}

const MissionMaterial* MissionMaterialDatabase::find(
    std::uint32_t mission_id, const std::string& stable_id) const noexcept {
  const auto it = std::find_if(materials_.begin(), materials_.end(),
                               [mission_id, &stable_id](const MissionMaterial& material) {
                                 return material.mission_id == mission_id &&
                                        material.stable_id == stable_id;
                               });
  return it == materials_.end() ? nullptr : &*it;
}

bool MissionTextureBinding::valid() const noexcept {
  return mission_id != 0 && !stable_id.empty() && !texture_id.empty() &&
         (sampler_filter == "nearest" || sampler_filter == "linear") &&
         (sampler_address == "wrap" || sampler_address == "clamp") &&
         content_hash != 0;
}

bool MissionTextureDatabase::add(MissionTextureBinding texture) {
  if (!texture.valid() || find(texture.mission_id, texture.stable_id) != nullptr) {
    return false;
  }
  textures_.push_back(std::move(texture));
  return true;
}

bool MissionTextureDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionTextureDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    const auto fifth = fourth == std::string::npos ? std::string::npos : line.find('\t', fourth + 1);
    const auto sixth = fifth == std::string::npos ? std::string::npos : line.find('\t', fifth + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        fifth == std::string::npos) {
      return false;
    }
    MissionTextureBinding texture;
    if (!detail::parse_u32(std::string_view(line).substr(0, first), texture.mission_id) ||
        (texture.stable_id = line.substr(first + 1, second - first - 1)).empty() ||
        (texture.texture_id = line.substr(second + 1, third - second - 1)).empty() ||
        (texture.sampler_filter = line.substr(third + 1, fourth - third - 1)).empty() ||
        (texture.sampler_address = line.substr(fourth + 1, fifth - fourth - 1)).empty() ||
        !detail::parse_hex_u64(std::string_view(line).substr(fifth + 1,
                                                       sixth == std::string::npos ?
                                                           std::string::npos : sixth - fifth - 1),
                       texture.content_hash)) {
      return false;
    }
    if (sixth != std::string::npos) {
      const auto seventh = line.find('\t', sixth + 1);
      if (seventh == std::string::npos ||
          (texture.source_path = line.substr(sixth + 1, seventh - sixth - 1)).empty() ||
          !detail::parse_u64(std::string_view(line).substr(seventh + 1,
                                                   line.find('\t', seventh + 1) == std::string::npos ?
                                                       std::string::npos : line.find('\t', seventh + 1) - seventh - 1),
                     texture.source_size) ||
          texture.source_size == 0) return false;
      const auto eighth = line.find('\t', seventh + 1);
      if (eighth != std::string::npos) {
        const auto ninth = line.find('\t', eighth + 1);
        const auto tenth = ninth == std::string::npos ? std::string::npos : line.find('\t', ninth + 1);
        const auto eleventh = tenth == std::string::npos ? std::string::npos : line.find('\t', tenth + 1);
        if (ninth == std::string::npos || tenth == std::string::npos ||
            !detail::parse_u32(std::string_view(line).substr(eighth + 1, ninth - eighth - 1), texture.source_width) ||
            !detail::parse_u32(std::string_view(line).substr(ninth + 1, tenth - ninth - 1), texture.source_height) ||
            !detail::parse_u32(std::string_view(line).substr(tenth + 1,
                                                     eleventh == std::string::npos ?
                                                         std::string::npos : eleventh - tenth - 1),
                       texture.source_format) ||
            texture.source_width == 0 || texture.source_height == 0 || texture.source_format == 0) return false;
        if (eleventh != std::string::npos &&
            (!detail::parse_u64(std::string_view(line).substr(eleventh + 1), texture.gidx) || texture.gidx == 0)) return false;
      }
      if (texture.source_path.is_relative()) texture.source_path = manifest.parent_path() / texture.source_path;
      std::error_code error;
      if (!std::filesystem::is_regular_file(texture.source_path, error) || error ||
          std::filesystem::file_size(texture.source_path, error) != texture.source_size || error) {
        return false;
      }
      std::uint64_t source_hash = 0;
      if (!file_fnv64(texture.source_path, source_hash) || source_hash != texture.content_hash) {
        return false;
      }
      if (texture.source_path.extension() == ".ppm") {
        std::ifstream image(texture.source_path, std::ios::binary);
        std::string magic;
        std::uint32_t width = 0, height = 0, max_value = 0;
        image >> magic >> width >> height >> max_value;
        if (!image || magic != "P6" || width == 0 || height == 0 || max_value != 255) return false;
        image.get();
        std::vector<unsigned char> bytes(static_cast<std::size_t>(width) * height * 3u);
        image.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!image) return false;
        Image decoded{width, height, {}};
        decoded.pixels.resize(static_cast<std::size_t>(width) * height);
        for (std::size_t pixel = 0; pixel < decoded.pixels.size(); ++pixel) {
          decoded.pixels[pixel] = 0xFF000000u |
              (static_cast<std::uint32_t>(bytes[pixel * 3u]) << 16u) |
              (static_cast<std::uint32_t>(bytes[pixel * 3u + 1u]) << 8u) |
              static_cast<std::uint32_t>(bytes[pixel * 3u + 2u]);
        }
        loaded.images_[std::to_string(texture.mission_id) + ":" + texture.stable_id] = std::move(decoded);
      }
      if (texture.source_width != 0) {
        std::array<unsigned char, 0x28> header{};
        std::ifstream source(texture.source_path, std::ios::binary);
        source.read(reinterpret_cast<char*>(header.data()),
                    static_cast<std::streamsize>(header.size()));
        if (!source || std::memcmp(header.data(), "NTXR", 4) != 0) return false;
        const auto be16 = [&header](std::size_t offset) {
          return static_cast<std::uint32_t>(header[offset]) << 8u |
                 static_cast<std::uint32_t>(header[offset + 1]);
        };
        const auto be_format = [&header](std::size_t offset) {
          return static_cast<std::uint32_t>(header[offset]) << 8u |
                 static_cast<std::uint32_t>(header[offset + 1]);
        };
        if (be16(0x24) != texture.source_width || be16(0x26) != texture.source_height ||
            be_format(0x04) != texture.source_format) return false;
      }
    }
    if (!loaded.add(std::move(texture))) return false;
  }
  textures_ = std::move(loaded.textures_);
  images_ = std::move(loaded.images_);
  return true;
}

const MissionTextureBinding* MissionTextureDatabase::find(
    std::uint32_t mission_id, const std::string& stable_id) const noexcept {
  const auto it = std::find_if(textures_.begin(), textures_.end(),
                               [mission_id, &stable_id](const MissionTextureBinding& texture) {
                                 return texture.mission_id == mission_id &&
                                        texture.stable_id == stable_id;
                               });
  return it == textures_.end() ? nullptr : &*it;
}

bool MissionTextureDatabase::sample(std::uint32_t mission_id, const std::string& stable_id,
                                    float u, float v, std::uint32_t& rgba) const noexcept {
  const auto it = images_.find(std::to_string(mission_id) + ":" + stable_id);
  const MissionTextureBinding* binding = find(mission_id, stable_id);
  if (it == images_.end() || binding == nullptr || it->second.width == 0 ||
      it->second.height == 0 || !std::isfinite(u) || !std::isfinite(v)) return false;
  if (binding->sampler_address == "clamp") {
    u = std::clamp(u, 0.0f, std::nextafter(1.0f, 0.0f));
    v = std::clamp(v, 0.0f, std::nextafter(1.0f, 0.0f));
  } else {
    u -= std::floor(u);
    v -= std::floor(v);
  }
  const float width = static_cast<float>(it->second.width);
  const float height = static_cast<float>(it->second.height);
  const std::uint32_t x = std::min(
      it->second.width - 1u, static_cast<std::uint32_t>(u * width));
  const std::uint32_t y = std::min(
      it->second.height - 1u, static_cast<std::uint32_t>(v * height));
  if (binding->sampler_filter == "nearest") {
    rgba = it->second.pixels[static_cast<std::size_t>(y) * it->second.width + x];
    return true;
  }
  const float px = binding->sampler_address == "clamp"
      ? u * static_cast<float>(it->second.width - 1u)
      : u * width;
  const float py = binding->sampler_address == "clamp"
      ? v * static_cast<float>(it->second.height - 1u)
      : v * height;
  const std::uint32_t x0 = static_cast<std::uint32_t>(std::floor(px)) % it->second.width;
  const std::uint32_t y0 = static_cast<std::uint32_t>(std::floor(py)) % it->second.height;
  const std::uint32_t x1 = binding->sampler_address == "clamp" ?
      std::min(x0 + 1u, it->second.width - 1u) : (x0 + 1u) % it->second.width;
  const std::uint32_t y1 = binding->sampler_address == "clamp" ?
      std::min(y0 + 1u, it->second.height - 1u) : (y0 + 1u) % it->second.height;
  const float fx = px - std::floor(px), fy = py - std::floor(py);
  const auto fetch = [&it](std::uint32_t sx, std::uint32_t sy) {
    return it->second.pixels[static_cast<std::size_t>(sy) * it->second.width + sx];
  };
  const std::uint32_t c00 = fetch(x0, y0), c10 = fetch(x1, y0);
  const std::uint32_t c01 = fetch(x0, y1), c11 = fetch(x1, y1);
  std::uint32_t result = 0xFF000000u;
  for (unsigned channel = 0; channel < 3; ++channel) {
    const unsigned shift = 16u - channel * 8u;
    const float top = static_cast<float>((c00 >> shift) & 0xFFu) * (1.0f - fx) +
                      static_cast<float>((c10 >> shift) & 0xFFu) * fx;
    const float bottom = static_cast<float>((c01 >> shift) & 0xFFu) * (1.0f - fx) +
                         static_cast<float>((c11 >> shift) & 0xFFu) * fx;
    result |= static_cast<std::uint32_t>(std::clamp(top * (1.0f - fy) + bottom * fy,
                                                    0.0f, 255.0f)) << shift;
  }
  rgba = result;
  return true;
}

bool ShaderPermutation::valid() const noexcept {
  return !id.empty() && !vertex_layout.empty() && texture_fetches != 0 &&
         constant_count != 0 &&
         (render_target_format == "rgba8" || render_target_format == "rgba16f" ||
          render_target_format == "d24s8");
}

bool ShaderPermutationDatabase::add(ShaderPermutation permutation) {
  if (!permutation.valid() || find(permutation.id) != nullptr) {
    return false;
  }
  permutations_.push_back(std::move(permutation));
  return true;
}

bool ShaderPermutationDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  ShaderPermutationDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        line.find('\t', fourth + 1) != std::string::npos) {
      return false;
    }
    ShaderPermutation permutation;
    permutation.id = line.substr(0, first);
    permutation.vertex_layout = line.substr(first + 1, second - first - 1);
    if (!detail::parse_u32(std::string_view(line).substr(second + 1, third - second - 1),
                           permutation.texture_fetches) ||
        !detail::parse_u32(std::string_view(line).substr(third + 1, fourth - third - 1),
                           permutation.constant_count) ||
        (permutation.render_target_format = line.substr(fourth + 1)).empty()) {
      return false;
    }
    if (!loaded.add(std::move(permutation))) {
      return false;
    }
  }
  permutations_ = std::move(loaded.permutations_);
  return true;
}

const ShaderPermutation* ShaderPermutationDatabase::find(const std::string& id) const noexcept {
  const auto it = std::find_if(permutations_.begin(), permutations_.end(),
                               [&id](const ShaderPermutation& permutation) {
                                 return permutation.id == id;
                               });
  return it == permutations_.end() ? nullptr : &*it;
}

bool MissionRenderTargetDefinition::valid() const noexcept {
  constexpr std::uint32_t max_dimension = 4096;
  const bool supported_sample_count =
      sample_count == 1 || sample_count == 2 || sample_count == 4 || sample_count == 8;
  return mission_id != 0 &&
         (target_id == "world_color" || target_id == "present" || target_id == "main_color") &&
         width != 0 && height != 0 &&
         width <= max_dimension && height <= max_dimension &&
         static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) <=
             16u * 1024u * 1024u &&
         supported_sample_count &&
         (color_format == "rgba8" || color_format == "rgba16f") &&
         (depth_format == "none" || depth_format == "d24s8") &&
         (depth_enabled == (depth_format != "none"));
}

bool MissionRenderTargetDatabase::add(MissionRenderTargetDefinition definition) {
  if (!definition.valid() || find(definition.mission_id, definition.target_id) != nullptr) return false;
  targets_.push_back(std::move(definition));
  return true;
}

bool MissionRenderTargetDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionRenderTargetDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    const auto fifth = fourth == std::string::npos ? std::string::npos : line.find('\t', fourth + 1);
    const auto sixth = fifth == std::string::npos ? std::string::npos : line.find('\t', fifth + 1);
    const auto seventh = sixth == std::string::npos ? std::string::npos : line.find('\t', sixth + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        fifth == std::string::npos || sixth == std::string::npos ||
        seventh == std::string::npos || line.find('\t', seventh + 1) != std::string::npos) {
      return false;
    }
    MissionRenderTargetDefinition definition;
    if (!detail::parse_u32(std::string_view(line).substr(0, first), definition.mission_id) ||
        (definition.target_id = line.substr(first + 1, second - first - 1)).empty() ||
        !detail::parse_u32(std::string_view(line).substr(second + 1, third - second - 1),
                   definition.width) ||
        !detail::parse_u32(std::string_view(line).substr(third + 1, fourth - third - 1),
                   definition.height) ||
        !detail::parse_u32(std::string_view(line).substr(fourth + 1, fifth - fourth - 1),
                   definition.sample_count) ||
        (definition.color_format = line.substr(fifth + 1, sixth - fifth - 1)).empty() ||
        (definition.depth_format = line.substr(sixth + 1, seventh - sixth - 1)).empty() ||
        !detail::parse_bool01(std::string_view(line).substr(seventh + 1), definition.depth_enabled) ||
        !loaded.add(std::move(definition))) {
      return false;
    }
  }
  targets_ = std::move(loaded.targets_);
  return true;
}

const MissionRenderTargetDefinition* MissionRenderTargetDatabase::find(
    std::uint32_t mission_id) const noexcept {
  return find(mission_id, "main_color");
}

const MissionRenderTargetDefinition* MissionRenderTargetDatabase::find(
    std::uint32_t mission_id, const std::string& target_id) const noexcept {
  const auto it = std::find_if(targets_.begin(), targets_.end(),
                               [mission_id, &target_id](const MissionRenderTargetDefinition& target) {
                                 return target.mission_id == mission_id &&
                                        target.target_id == target_id;
                               });
  return it == targets_.end() ? nullptr : &*it;
}

bool MissionRenderPass::valid() const noexcept {
  return mission_id != 0 && !pass_id.empty() && order != 0 &&
         (color_target == "main_color" || color_target == "world_color") &&
         (depth_target == "main_depth" || depth_target == "none") &&
         std::isfinite(clear_depth) && clear_depth >= 0.0f && clear_depth <= 1.0f;
}

bool MissionRenderPassDatabase::add(MissionRenderPass pass) {
  if (!pass.valid() || find(pass.mission_id, pass.pass_id) != nullptr) return false;
  passes_.push_back(std::move(pass));
  return true;
}

bool MissionRenderPassDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionRenderPassDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    const auto fifth = fourth == std::string::npos ? std::string::npos : line.find('\t', fourth + 1);
    const auto sixth = fifth == std::string::npos ? std::string::npos : line.find('\t', fifth + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        fifth == std::string::npos || sixth == std::string::npos ||
        line.find('\t', sixth + 1) != std::string::npos) {
      return false;
    }
    MissionRenderPass pass;
    if (!detail::parse_u32(std::string_view(line).substr(0, first), pass.mission_id) ||
        (pass.pass_id = line.substr(first + 1, second - first - 1)).empty() ||
        !detail::parse_u32(std::string_view(line).substr(second + 1, third - second - 1), pass.order) ||
        (pass.color_target = line.substr(third + 1, fourth - third - 1)).empty() ||
        (pass.depth_target = line.substr(fourth + 1, fifth - fourth - 1)).empty() ||
        !detail::parse_hex_u32(std::string_view(line).substr(fifth + 1, sixth - fifth - 1),
                       pass.clear_color) ||
        !detail::parse_f32(std::string_view(line).substr(sixth + 1), pass.clear_depth) ||
        !loaded.add(std::move(pass))) {
      return false;
    }
  }
  passes_ = std::move(loaded.passes_);
  return true;
}

const MissionRenderPass* MissionRenderPassDatabase::find(
    std::uint32_t mission_id, const std::string& pass_id) const noexcept {
  const auto it = std::find_if(passes_.begin(), passes_.end(),
                               [mission_id, &pass_id](const MissionRenderPass& pass) {
                                 return pass.mission_id == mission_id && pass.pass_id == pass_id;
                               });
  return it == passes_.end() ? nullptr : &*it;
}

bool MissionRenderResolve::valid() const noexcept {
  return mission_id != 0 && source_pass == "world" &&
         (source_target == "main_color" || source_target == "world_color") &&
         destination_target == "present" &&
         (mode == "copy" || mode == "tonemap" || mode == "linear" || mode == "msaa_resolve");
}

bool MissionRenderResolveDatabase::add(MissionRenderResolve resolve) {
  if (!resolve.valid() || find(resolve.mission_id, resolve.source_pass) != nullptr) return false;
  resolves_.push_back(std::move(resolve));
  return true;
}

bool MissionRenderResolveDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  MissionRenderResolveDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    const auto fourth = third == std::string::npos ? std::string::npos : line.find('\t', third + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || fourth == std::string::npos ||
        line.find('\t', fourth + 1) != std::string::npos) {
      return false;
    }
    MissionRenderResolve resolve;
    if (!detail::parse_u32(std::string_view(line).substr(0, first), resolve.mission_id) ||
        (resolve.source_pass = line.substr(first + 1, second - first - 1)).empty() ||
        (resolve.source_target = line.substr(second + 1, third - second - 1)).empty() ||
        (resolve.destination_target = line.substr(third + 1, fourth - third - 1)).empty() ||
        (resolve.mode = line.substr(fourth + 1)).empty()) {
      return false;
    }
    if (!loaded.add(std::move(resolve))) {
      return false;
    }
  }
  resolves_ = std::move(loaded.resolves_);
  return true;
}

const MissionRenderResolve* MissionRenderResolveDatabase::find(
    std::uint32_t mission_id, const std::string& source_pass) const noexcept {
  const auto it = std::find_if(resolves_.begin(), resolves_.end(),
                               [mission_id, &source_pass](const MissionRenderResolve& resolve) {
                                 return resolve.mission_id == mission_id &&
                                        resolve.source_pass == source_pass;
                               });
  return it == resolves_.end() ? nullptr : &*it;
}

bool QualifiedBufferDatabase::add(QualifiedBufferRecord record) {
  if (record.buffer_id.empty() || record.path.empty() || record.byte_size == 0 ||
      record.fnv64 == 0 || find(record.buffer_id) != nullptr) {
    return false;
  }
  record.verified = false;
  buffers_.push_back(std::move(record));
  return true;
}

bool QualifiedBufferDatabase::load_manifest(const std::filesystem::path& manifest) {
  std::ifstream input(manifest);
  if (!input) return false;
  QualifiedBufferDatabase loaded;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    const auto first = line.find('\t');
    const auto second = first == std::string::npos ? std::string::npos : line.find('\t', first + 1);
    const auto third = second == std::string::npos ? std::string::npos : line.find('\t', second + 1);
    if (first == std::string::npos || second == std::string::npos ||
        third == std::string::npos || line.find('\t', third + 1) != std::string::npos) {
      return false;
    }
    QualifiedBufferRecord record;
    record.buffer_id = line.substr(0, first);
    record.path = line.substr(first + 1, second - first - 1);
    if (record.path.is_relative()) record.path = manifest.parent_path() / record.path;
    if (!detail::parse_u64(std::string_view(line).substr(second + 1, third - second - 1),
                           record.byte_size) ||
        !detail::parse_u64(std::string_view(line).substr(third + 1), record.fnv64)) {
      return false;
    }
    if (!loaded.add(std::move(record))) {
      return false;
    }
  }
  buffers_ = std::move(loaded.buffers_);
  return true;
}

bool QualifiedBufferDatabase::verify(const std::string& buffer_id) {
  QualifiedBufferRecord* record = nullptr;
  for (QualifiedBufferRecord& candidate : buffers_) {
    if (candidate.buffer_id == buffer_id) {
      record = &candidate;
      break;
    }
  }
  if (record == nullptr) return false;
  std::ifstream input(record->path, std::ios::binary);
  if (!input) return false;
  std::uint64_t hash = 1469598103934665603ull;
  std::uint64_t size = 0;
  char byte = 0;
  while (input.get(byte)) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= 1099511628211ull;
    ++size;
  }
  record->verified = size == record->byte_size && hash == record->fnv64;
  return record->verified;
}

bool QualifiedBufferDatabase::has_verified(const std::string& buffer_id) const noexcept {
  const QualifiedBufferRecord* record = find(buffer_id);
  return record != nullptr && record->verified;
}

const QualifiedBufferRecord* QualifiedBufferDatabase::find(
    const std::string& buffer_id) const noexcept {
  const auto it = std::find_if(buffers_.begin(), buffers_.end(),
                               [&buffer_id](const QualifiedBufferRecord& record) {
                                 return record.buffer_id == buffer_id;
                               });
  return it == buffers_.end() ? nullptr : &*it;
}

}  // namespace ac6
