#include "ac6/product_runtime.h"
#include "text_parse.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>

namespace ac6 {
namespace {

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
        const auto bef32 = [&be32](std::size_t offset, float& value) {
          std::uint32_t bits = 0;
          if (!be32(offset, bits)) return false;
          std::memcpy(&value, &bits, sizeof(value));
          return std::isfinite(value);
        };
        std::uint32_t declared_size = 0, header_size = 0, polygon_size = 0;
        std::uint32_t vertex_size = 0, additional_size = 0;
        std::uint16_t object_count = 0;
        if (!be32(4, declared_size) || declared_size != raw.size() || !be16(0x0a, object_count) ||
            object_count == 0 || !be32(0x10, header_size) ||
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
          }
          const std::uint32_t vertex_base_index = polygon.vertex_offset / vertex_stride;
          for (std::uint32_t index = 0; index < polygon.index_count; ++index) {
            std::uint16_t local = 0;
            if (!be16(polygon_base + polygon.index_offset + static_cast<std::size_t>(index) * 2u, local)) return false;
            if (local == 0xffffu) {
              // Preserve the restart boundary: retail NDXR polygons are
              // triangle strips, not a flat triangle-list stream.
                decoded.indices.push_back(std::numeric_limits<std::uint32_t>::max());
              continue;
            }
            if (local >= polygon.vertex_count ||
                vertex_base_index >= metadata.vertex_count ||
                local >= metadata.vertex_count - vertex_base_index) {
              return false;
            }
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
  if (!detail::parse_u32(std::string_view(line).substr(second + 1, third - second - 1),
                 metadata.vertex_count) ||
      !detail::parse_u32(std::string_view(line).substr(third + 1, fourth - third - 1),
                 metadata.index_count) ||
      !detail::parse_u32(std::string_view(line).substr(fourth + 1), metadata.primitive_count)) {
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
    if (!detail::parse_u32(std::string_view(line).substr(section_first + 1,
                                                 section_second - section_first - 1), count) ||
        !detail::parse_u32(std::string_view(line).substr(section_second + 1), stride_or_flags)) {
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
      metadata.vertex_byte_size > payload_byte_size ||
      metadata.index_byte_size > payload_byte_size - metadata.vertex_byte_size) {
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
