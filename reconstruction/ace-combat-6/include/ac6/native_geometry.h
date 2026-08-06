#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace ac6 {

class MissionDrawable;
class QualifiedBufferDatabase;
struct QualifiedBufferRecord;

enum class NativeIndexTopology : std::uint8_t {
  TriangleList,
  TriangleStripRestart,
};

struct NativeGeometryMetadata {
  std::string buffer_id;
  std::string source_format;
  NativeIndexTopology topology{NativeIndexTopology::TriangleList};
  std::uint32_t vertex_count{};
  std::uint32_t index_count{};
  std::uint32_t primitive_count{};
  std::uint32_t vertex_section_count{};
  std::uint32_t index_section_count{};
  std::uint32_t polygon_descriptor_count{};
  std::uint32_t vertex_stride{};
  std::uint32_t index_size{};
  std::uint64_t vertex_byte_size{};
  std::uint64_t index_byte_size{};
};

struct DecodedVertex {
  float x{};
  float y{};
  float z{};
  float u{};
  float v{};
};

struct DecodedGeometryBounds {
  float min_x{};
  float min_y{};
  float min_z{};
  float max_x{};
  float max_y{};
  float max_z{};
  bool valid{};
};

struct DecodedGeometry {
  std::string buffer_id;
  std::vector<DecodedVertex> vertices;
  std::vector<std::uint32_t> indices;
  DecodedGeometryBounds bounds;
};

class NativeGeometryDatabase final {
 public:
  bool load_verified(const MissionDrawable& drawable, const QualifiedBufferDatabase& buffers);
  const NativeGeometryMetadata* find(const std::string& buffer_id) const noexcept;
  const DecodedGeometry* decoded(const std::string& buffer_id) const noexcept;

 private:
  bool load_verified_binary(const MissionDrawable& drawable,
                            const std::vector<unsigned char>& raw);
  bool load_verified_text(const MissionDrawable& drawable,
                          const QualifiedBufferRecord& record,
                          std::ifstream& input);
  std::vector<NativeGeometryMetadata> geometries_;
  std::vector<DecodedGeometry> decoded_;
};

}  // namespace ac6
