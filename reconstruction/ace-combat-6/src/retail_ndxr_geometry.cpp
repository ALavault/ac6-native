#include "ac6/retail_ndxr_geometry.h"

#include <cmath>
#include <cstring>

namespace ac6::retail {
namespace {

float be_float(const std::uint8_t* at) noexcept {
  const std::uint32_t word = (static_cast<std::uint32_t>(at[0]) << 24) |
                             (static_cast<std::uint32_t>(at[1]) << 16) |
                             (static_cast<std::uint32_t>(at[2]) << 8) | at[3];
  float value = 0.0F;
  std::memcpy(&value, &word, sizeof(value));
  return value;
}

std::uint16_t be_u16(const std::uint8_t* at) noexcept {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(at[0]) << 8) | at[1]);
}

// IEEE 754 binary16, big-endian. Normals are stored as four of these.
float be_half(const std::uint8_t* at) noexcept {
  const std::uint16_t bits = be_u16(at);
  const std::uint32_t sign = static_cast<std::uint32_t>(bits >> 15) & 1u;
  std::uint32_t exponent = static_cast<std::uint32_t>(bits >> 10) & 0x1Fu;
  std::uint32_t mantissa = static_cast<std::uint32_t>(bits) & 0x3FFu;
  std::uint32_t out = 0;
  if (exponent == 0) {
    if (mantissa == 0) {
      out = sign << 31;                                   // +-0
    } else {
      exponent = 127 - 15 + 1;                            // subnormal
      while ((mantissa & 0x400u) == 0) { mantissa <<= 1; --exponent; }
      mantissa &= 0x3FFu;
      out = (sign << 31) | (exponent << 23) | (mantissa << 13);
    }
  } else if (exponent == 31) {
    out = (sign << 31) | 0x7F800000u | (mantissa << 13);   // inf / NaN
  } else {
    out = (sign << 31) | ((exponent - 15 + 127) << 23) | (mantissa << 13);
  }
  float value = 0.0F;
  std::memcpy(&value, &out, sizeof(value));
  return value;
}

}  // namespace

std::optional<NdxrMesh> decode_ndxr_descriptor(const NdxrContainer& container,
                                               const std::uint8_t* bytes,
                                               std::size_t size,
                                               const NdxrDescriptor& descriptor) noexcept {
  if (bytes == nullptr) return std::nullopt;
  if (descriptor.vertex_stride == 0) return std::nullopt;   // format outside T8/T18
  if (descriptor.vertex_count == 0 || descriptor.index_count == 0) return std::nullopt;

  const NdxrSections& sections = container.sections();
  const std::size_t vertex_base = sections.second + descriptor.vertex_offset;
  const std::size_t vertex_bytes =
      static_cast<std::size_t>(descriptor.vertex_count) * descriptor.vertex_stride;
  const std::size_t index_base = sections.first + descriptor.index_offset;
  const std::size_t index_bytes = static_cast<std::size_t>(descriptor.index_count) * 2u;
  if (vertex_base + vertex_bytes > size) return std::nullopt;
  if (index_base + index_bytes > size) return std::nullopt;
  // The stride must hold three floats before anything is read out of it.
  if (descriptor.vertex_stride < 12u) return std::nullopt;

  NdxrMesh mesh;
  mesh.positions.reserve(descriptor.vertex_count);
  for (std::uint32_t vertex = 0; vertex < descriptor.vertex_count; ++vertex) {
    const std::uint8_t* at = bytes + vertex_base +
                             static_cast<std::size_t>(vertex) * descriptor.vertex_stride;
    NdxrPosition position{be_float(at), be_float(at + 4), be_float(at + 8)};
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(position.z)) {
      return std::nullopt;
    }
    mesh.positions.push_back(position);
  }

  // NORMAL and TEXCOORD, when the stride has room. The offsets are the element
  // tables' (see the header); the component types were measured rather than
  // decoded from the Xenos format words.
  const std::size_t texcoord_at = descriptor.vertex_stride >= 32u
                                      ? kTexcoordOffset32 : kTexcoordOffset28;
  const bool has_normal = descriptor.vertex_stride >= kNormalOffset + 8u;
  const bool has_texcoord = descriptor.vertex_stride >= texcoord_at + 8u;
  if (has_normal) mesh.normals.reserve(descriptor.vertex_count);
  if (has_texcoord) mesh.texcoords.reserve(descriptor.vertex_count);
  for (std::uint32_t vertex = 0; vertex < descriptor.vertex_count; ++vertex) {
    const std::uint8_t* at = bytes + vertex_base +
                             static_cast<std::size_t>(vertex) * descriptor.vertex_stride;
    if (has_normal) {
      mesh.normals.push_back({be_half(at + kNormalOffset),
                              be_half(at + kNormalOffset + 2),
                              be_half(at + kNormalOffset + 4)});
    }
    if (has_texcoord) {
      mesh.texcoords.push_back({be_float(at + texcoord_at),
                                be_float(at + texcoord_at + 4)});
    }
  }

  mesh.indices.reserve(descriptor.index_count);
  for (std::uint32_t index = 0; index < descriptor.index_count; ++index) {
    const std::uint16_t value = be_u16(bytes + index_base + std::size_t(index) * 2u);
    // 0xFFFF breaks the strip; every other value must address this
    // descriptor's own vertices, which is the reading that scored 1227/1227.
    if (value != kStripRestart && value >= descriptor.vertex_count) return std::nullopt;
    mesh.indices.push_back(value);
  }

  NdxrBounds bounds{};
  bounds.min_x = bounds.max_x = mesh.positions[0].x;
  bounds.min_y = bounds.max_y = mesh.positions[0].y;
  bounds.min_z = bounds.max_z = mesh.positions[0].z;
  for (const NdxrPosition& position : mesh.positions) {
    bounds.min_x = std::fmin(bounds.min_x, position.x);
    bounds.min_y = std::fmin(bounds.min_y, position.y);
    bounds.min_z = std::fmin(bounds.min_z, position.z);
    bounds.max_x = std::fmax(bounds.max_x, position.x);
    bounds.max_y = std::fmax(bounds.max_y, position.y);
    bounds.max_z = std::fmax(bounds.max_z, position.z);
  }
  bounds.valid = true;
  mesh.bounds = bounds;
  return mesh;
}

}  // namespace ac6::retail
