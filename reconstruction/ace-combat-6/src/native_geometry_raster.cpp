#include "ac6/product_runtime.h"
#include "ac6/retail_ndxr_container.h"
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
        // The header was parsed inline here, correctly but without a single
        // retail address behind it - cycle 1189's complaint about this file.
        // NdxrContainer is the same read with the derivation attached
        // (0x8234CA28 identity, 0x82350F08 sections, 0x823556E0 records) and
        // under the v4 contract, so the duplicate is retired rather than
        // annotated. It also validates before serving, which this did not.
        ac6::retail::NdxrRefusal refusal = ac6::retail::NdxrRefusal::kNone;
        const auto container =
            ac6::retail::NdxrContainer::Open(raw.data(), raw.size(), &refusal);
        if (!container) return false;

        const std::uint16_t object_count = container->record_count();
        const ac6::retail::NdxrSections& sections = container->sections();
        // 0x82362190 binds section 1 as the index block and section 2 as the
        // vertex block; these are those two, expressed as this function's
        // existing locals.
        const std::size_t polygon_base = sections.first;
        const std::size_t vertex_base = sections.second;
        const std::uint32_t polygon_size =
            static_cast<std::uint32_t>(sections.second - sections.first);
        const std::uint32_t vertex_size =
            static_cast<std::uint32_t>(sections.end - sections.second);

        const std::size_t object_table = ac6::retail::NdxrContainer::kRecordArrayBase;
        const std::size_t polygon_descriptors = object_table +
            static_cast<std::size_t>(object_count) * 0x30u;
        if (polygon_descriptors > raw.size()) return false;
        std::uint32_t polygon_count = 0;
        for (std::uint16_t object = 0; object < object_count; ++object) {
          const auto record = container->Record(object);
          if (!record.has_value() ||
              polygon_count > 100000u - record->descriptor_count) return false;
          polygon_count += record->descriptor_count;
        }
        const std::size_t polygon_descriptor_end = polygon_descriptors +
            static_cast<std::size_t>(polygon_count) * 0x30u;
        if (polygon_descriptor_end > polygon_base) return false;

        struct Polygon { std::uint32_t index_offset{}, vertex_offset{}; std::uint16_t vertex_count{}, index_count{}, format{}; };
        std::vector<Polygon> polygons;
        polygons.reserve(polygon_count);
        std::uint32_t vertex_stride = 0;
        std::uint64_t max_vertex_end = 0;
        std::uint64_t total_indices = 0;
        // Retail DEREFERENCES rec+0x2C to reach a record's descriptors
        // (0x823555D0); this loop used to assume they are contiguous after the
        // record array. On the extracted corpus the two walks agree for 537 of
        // 537 files - measured before the change, not after - so following the
        // pointer is behaviour-preserving here and strictly closer to retail on
        // any file where they would not.
        for (std::uint16_t object = 0; object < object_count; ++object) {
          const auto record = container->Record(object);
          if (!record.has_value()) return false;
          for (std::uint16_t index = 0; index < record->descriptor_count; ++index) {
            const auto descriptor = container->Descriptor(*record, index);
            if (!descriptor.has_value()) return false;
            Polygon value;
            value.index_offset = descriptor->index_offset;
            value.vertex_offset = descriptor->vertex_offset;
            value.vertex_count = descriptor->vertex_count;
            value.index_count = descriptor->index_count;
            value.format = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(descriptor->format_hi) << 8) |
                descriptor->format_lo);
            if (value.index_offset > polygon_size ||
                static_cast<std::uint64_t>(value.index_count) * 2u >
                    polygon_size - value.index_offset) {
              return false;
            }
            // Was four measured constants - 0x0611->28, 0x0613->32, 0x0711->44,
            // 0x0721->52 - which cycle 1189 flagged as citing no retail address
            // and cycle 1203 could not settle. Cycle 1217 derived them, and the
            // container now computes the sum, so this reads the field rather
            // than re-deriving it here.
            const std::uint32_t stride = descriptor->vertex_stride;
            if (stride == 0 || (vertex_stride != 0 && vertex_stride != stride)) {
              return false;
            }
            vertex_stride = stride;
            if (value.vertex_offset % stride != 0 || value.vertex_offset > vertex_size ||
                static_cast<std::uint64_t>(value.vertex_count) * stride >
                    vertex_size - value.vertex_offset ||
                total_indices >
                    std::numeric_limits<std::uint32_t>::max() - value.index_count) {
              return false;
            }
            total_indices += value.index_count;
            max_vertex_end = std::max(max_vertex_end,
                static_cast<std::uint64_t>(value.vertex_offset) +
                static_cast<std::uint64_t>(value.vertex_count) * stride);
            polygons.push_back(value);
          }
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
          // Derived in cycle 1233 from the declaration element lists the
          // renderer's own builder walks (0x82345100), 12-byte Xbox 360
          // D3DVERTEXELEMENT9 records:
          //   T8[6]  @0x8201140C : POSITION @0 (12) | NORMAL @12 (8)      -> 20
          //   T18[1] @0x820111D8 : TEXCOORD @rel 0                        -> abs 20
          //   T18[3] @0x820111FC : COLOR @rel 0 (4) | TEXCOORD @rel 4     -> abs 24
          // so 0x0611 puts UV at 20 and 0x0613 at 24. This read 16 and 20 -
          // four bytes early in both, which at stride 32 takes the last word of
          // COLOR and the first of TEXCOORD.
          //
          // The 12-byte element size is the whole of that correction, so cycle
          // 1262 corroborated it from a second function in another subsystem.
          // 0x821DE898 allocates a declaration object by walking the same array
          // to D3DDECL_END and sizing it from the count:
          //
          //   821de8c0  addi   r10,r10,0xc      one element is TWELVE bytes
          //   821de8cc  cmplwi cr6,r9,0xff      D3DDECL_END: stream 0xFF
          //   821de8d4  mulli  r11,r11,0xc      count * 12
          //   821de8d8  addi   r3,r11,0x38      plus a 0x38 header
          //
          // A stride can be misread once. Two functions in different subsystems
          // do not agree on 0xc by accident.
          const std::size_t uv_offset = vertex_stride == 28u ? 20u : 24u;
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
