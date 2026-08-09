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
