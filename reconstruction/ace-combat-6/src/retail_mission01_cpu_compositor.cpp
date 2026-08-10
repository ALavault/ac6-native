#include "ac6/retail_mission01_cpu_compositor.h"

#include "ac6/sha256.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <set>
#include <utility>

namespace ac6::retail {
namespace {

// 0x8225DF88 stamps 1.0 and 24000.0 into CAce6CameraManager+0xDC/+0xE0;
// 0x8209B398 passes those fields to the perspective builder. The CPU lane uses
// those retail bounds but does not claim retail's reversed-Z representation.
constexpr float kNearPlane = 1.0F;
constexpr float kFarPlane = 24000.0F;
constexpr std::uint32_t kMaxWidth = 4096;
constexpr std::uint32_t kMaxHeight = 2160;

struct Vec3 final {
  float x{};
  float y{};
  float z{};
};

Vec3 operator-(Vec3 left, Vec3 right) noexcept {
  return {left.x - right.x, left.y - right.y, left.z - right.z};
}

float dot(Vec3 left, Vec3 right) noexcept {
  return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vec3 cross(Vec3 left, Vec3 right) noexcept {
  return {left.y * right.z - left.z * right.y,
          left.z * right.x - left.x * right.z,
          left.x * right.y - left.y * right.x};
}

bool normalize(Vec3 &value) noexcept {
  const float length_squared = dot(value, value);
  if (!std::isfinite(length_squared) || length_squared <= 0.000001F) {
    return false;
  }
  const float inverse = 1.0F / std::sqrt(length_squared);
  value.x *= inverse;
  value.y *= inverse;
  value.z *= inverse;
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

Vec3 from_array(const std::array<float, 3> &value) noexcept {
  return {value[0], value[1], value[2]};
}

struct CameraProjection final {
  Vec3 eye;
  Vec3 right;
  Vec3 up;
  Vec3 forward;
  float tan_half_x{};
  float tan_half_y{};
  std::uint32_t width{};
  std::uint32_t height{};

  static bool request_is_bounded(const Mission01CpuFrameRequest &request,
                                 float fov_radians) noexcept {
    if (request.width == 0 || request.width > kMaxWidth ||
        request.height == 0 || request.height > kMaxHeight ||
        !std::isfinite(fov_radians) || fov_radians <= 0.0F ||
        fov_radians >= 3.14159265358979323846F) {
      return false;
    }
    return true;
  }

  bool finish(const Mission01CpuFrameRequest &request,
              float fov_radians) noexcept {
    tan_half_y = std::tan(fov_radians * 0.5F);
    tan_half_x = tan_half_y * (static_cast<float>(request.width) /
                               static_cast<float>(request.height));
    width = request.width;
    height = request.height;
    return std::isfinite(tan_half_x) && std::isfinite(tan_half_y) &&
           tan_half_x > 0.0F && tan_half_y > 0.0F;
  }

  static std::optional<CameraProjection>
  make_external(const Mission01CpuFrameRequest &request,
                float fov_radians) noexcept {
    if (!request_is_bounded(request, fov_radians))
      return std::nullopt;
    CameraProjection camera;
    camera.eye = from_array(request.pose.eye);
    camera.forward = from_array(request.pose.target) - camera.eye;
    Vec3 requested_up = from_array(request.pose.up);
    if (!normalize(camera.forward) || !normalize(requested_up)) {
      return std::nullopt;
    }
    camera.right = cross(camera.forward, requested_up);
    if (!normalize(camera.right))
      return std::nullopt;
    camera.up = cross(camera.right, camera.forward);
    if (!normalize(camera.up))
      return std::nullopt;
    return camera.finish(request, fov_radians)
               ? std::optional<CameraProjection>(camera)
               : std::nullopt;
  }

  static std::optional<CameraProjection>
  make_mode2(const Mission01CpuFrameRequest &request, float fov_radians,
             const RetailMode2CameraLocator &locator) noexcept {
    if (!request_is_bounded(request, fov_radians))
      return std::nullopt;
    CameraProjection camera;
    camera.eye = from_array(locator.position);
    camera.right = {locator.basis.rows[0][0], locator.basis.rows[0][1],
                    locator.basis.rows[0][2]};
    camera.up = {locator.basis.rows[1][0], locator.basis.rows[1][1],
                 locator.basis.rows[1][2]};
    camera.forward = {locator.basis.rows[2][0], locator.basis.rows[2][1],
                      locator.basis.rows[2][2]};
    const float right_length = dot(camera.right, camera.right);
    const float up_length = dot(camera.up, camera.up);
    const float forward_length = dot(camera.forward, camera.forward);
    const float right_up = dot(camera.right, camera.up);
    const float right_forward = dot(camera.right, camera.forward);
    const float up_forward = dot(camera.up, camera.forward);
    if (!std::isfinite(camera.eye.x) || !std::isfinite(camera.eye.y) ||
        !std::isfinite(camera.eye.z) || !std::isfinite(right_length) ||
        !std::isfinite(up_length) || !std::isfinite(forward_length) ||
        !std::isfinite(right_up) || !std::isfinite(right_forward) ||
        !std::isfinite(up_forward) || right_length < 0.99F ||
        right_length > 1.01F || up_length < 0.99F || up_length > 1.01F ||
        forward_length < 0.99F || forward_length > 1.01F ||
        std::abs(right_up) > 0.001F || std::abs(right_forward) > 0.001F ||
        std::abs(up_forward) > 0.001F) {
      return std::nullopt;
    }
    // Preserve the retail rows exactly. They are validated, not normalized or
    // rebuilt through a host cross product.
    return camera.finish(request, fov_radians)
               ? std::optional<CameraProjection>(camera)
               : std::nullopt;
  }

  Vec3 view(Vec3 world) const noexcept {
    const Vec3 relative = world - eye;
    return {dot(relative, right), dot(relative, up), dot(relative, forward)};
  }

  bool sphere_visible(Vec3 center, float radius) const noexcept {
    if (!std::isfinite(radius) || radius < 0.0F)
      return false;
    const Vec3 position = view(center);
    if (position.z + radius < kNearPlane || position.z - radius > kFarPlane) {
      return false;
    }
    const float horizontal_radius =
        radius * std::sqrt(1.0F + tan_half_x * tan_half_x);
    const float vertical_radius =
        radius * std::sqrt(1.0F + tan_half_y * tan_half_y);
    return std::abs(position.x) <=
               position.z * tan_half_x + horizontal_radius &&
           std::abs(position.y) <= position.z * tan_half_y + vertical_radius;
  }
};

struct WorldVertex final {
  Vec3 world;
  float u{};
  float v{};
};

struct ScreenVertex final {
  float x{};
  float y{};
  float depth{};
  float u{};
  float v{};
  float world_x{};
  float world_z{};
};

bool project(const CameraProjection &camera, const WorldVertex &source,
             ScreenVertex &target) noexcept {
  const Vec3 view = camera.view(source.world);
  if (!std::isfinite(view.x) || !std::isfinite(view.y) ||
      !std::isfinite(view.z) || view.z < kNearPlane || view.z > kFarPlane) {
    return false;
  }
  const float ndc_x = view.x / (view.z * camera.tan_half_x);
  const float ndc_y = view.y / (view.z * camera.tan_half_y);
  if (!std::isfinite(ndc_x) || !std::isfinite(ndc_y))
    return false;
  target = {(ndc_x * 0.5F + 0.5F) * static_cast<float>(camera.width),
            (0.5F - ndc_y * 0.5F) * static_cast<float>(camera.height),
            view.z,
            source.u,
            source.v,
            source.world.x,
            source.world.z};
  return true;
}

struct CpuSurface final {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t clear_color{};
  std::vector<std::uint32_t> color;
  std::vector<float> depth;
  bool water_query_failed{};
};

float edge(float ax, float ay, float bx, float by, float px,
           float py) noexcept {
  return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

float address_coordinate(float value,
                         Mission01CpuSamplerAddress address) noexcept {
  if (address == Mission01CpuSamplerAddress::Repeat) {
    value -= std::floor(value);
    return value < 0.0F ? value + 1.0F : value;
  }
  return std::clamp(value, 0.0F, 1.0F);
}

std::uint32_t sample_texture(const DecodedTexture &texture, float u, float v,
                             Mission01CpuSamplerAddress address) noexcept {
  if (texture.width == 0 || texture.height == 0 ||
      texture.pixels.size() !=
          static_cast<std::size_t>(texture.width) * texture.height) {
    return 0;
  }
  u = address_coordinate(u, address);
  v = address_coordinate(v, address);
  const std::uint32_t x = std::min(
      texture.width - 1,
      static_cast<std::uint32_t>(u * static_cast<float>(texture.width)));
  const std::uint32_t y = std::min(
      texture.height - 1,
      static_cast<std::uint32_t>(v * static_cast<float>(texture.height)));
  const std::uint32_t abgr =
      texture.pixels[static_cast<std::size_t>(y) * texture.width + x];
  return (abgr & 0xFF000000u) | ((abgr & 0x000000FFu) << 16) |
         (abgr & 0x0000FF00u) | ((abgr & 0x00FF0000u) >> 16);
}

enum class RasterDomain : std::uint8_t {
  Terrain,
  City,
};

std::size_t rasterize_triangle(CpuSurface &surface,
                               const CameraProjection &camera,
                               const std::array<WorldVertex, 3> &world,
                               const DecodedTexture &texture,
                               RasterDomain domain,
                               const Mission01CpuFrameRequest &request,
                               const Mission01WaterRenderResource &water,
                               Mission01CpuFrameReport &report) noexcept {
  std::array<ScreenVertex, 3> vertex;
  for (std::size_t index = 0; index < vertex.size(); ++index) {
    if (!project(camera, world[index], vertex[index]))
      return 0;
  }
  const float area = edge(vertex[0].x, vertex[0].y, vertex[1].x, vertex[1].y,
                          vertex[2].x, vertex[2].y);
  if (!std::isfinite(area) || std::abs(area) < 0.000001F)
    return 0;
  const float raw_min_x = std::min({vertex[0].x, vertex[1].x, vertex[2].x});
  const float raw_max_x = std::max({vertex[0].x, vertex[1].x, vertex[2].x});
  const float raw_min_y = std::min({vertex[0].y, vertex[1].y, vertex[2].y});
  const float raw_max_y = std::max({vertex[0].y, vertex[1].y, vertex[2].y});
  if (raw_max_x < 0.0F || raw_max_y < 0.0F ||
      raw_min_x >= static_cast<float>(surface.width) ||
      raw_min_y >= static_cast<float>(surface.height)) {
    return 0;
  }
  const std::uint32_t min_x =
      static_cast<std::uint32_t>(std::max(0.0F, std::floor(raw_min_x)));
  const std::uint32_t max_x = static_cast<std::uint32_t>(
      std::min(static_cast<float>(surface.width - 1), std::ceil(raw_max_x)));
  const std::uint32_t min_y =
      static_cast<std::uint32_t>(std::max(0.0F, std::floor(raw_min_y)));
  const std::uint32_t max_y = static_cast<std::uint32_t>(
      std::min(static_cast<float>(surface.height - 1), std::ceil(raw_max_y)));
  const float inverse_area = 1.0F / area;
  std::size_t writes = 0;
  for (std::uint32_t y = min_y; y <= max_y; ++y) {
    for (std::uint32_t x = min_x; x <= max_x; ++x) {
      const float px = static_cast<float>(x) + 0.5F;
      const float py = static_cast<float>(y) + 0.5F;
      const float w0 =
          edge(vertex[1].x, vertex[1].y, vertex[2].x, vertex[2].y, px, py) *
          inverse_area;
      const float w1 =
          edge(vertex[2].x, vertex[2].y, vertex[0].x, vertex[0].y, px, py) *
          inverse_area;
      const float w2 = 1.0F - w0 - w1;
      if (w0 < -0.00001F || w1 < -0.00001F || w2 < -0.00001F)
        continue;
      const float reciprocal_depth =
          w0 / vertex[0].depth + w1 / vertex[1].depth + w2 / vertex[2].depth;
      if (!std::isfinite(reciprocal_depth) || reciprocal_depth <= 0.0F) {
        continue;
      }
      const float depth = 1.0F / reciprocal_depth;
      const std::size_t pixel = static_cast<std::size_t>(y) * surface.width + x;
      if (depth >= surface.depth[pixel])
        continue;
      const auto interpolate = [&](float a, float b, float c) noexcept {
        return (w0 * a / vertex[0].depth + w1 * b / vertex[1].depth +
                w2 * c / vertex[2].depth) /
               reciprocal_depth;
      };
      const float u = interpolate(vertex[0].u, vertex[1].u, vertex[2].u);
      const float v = interpolate(vertex[0].v, vertex[1].v, vertex[2].v);
      std::uint32_t color = 0;
      bool water_bit = false;
      if (domain == RasterDomain::Terrain) {
        const float world_x = interpolate(vertex[0].world_x, vertex[1].world_x,
                                          vertex[2].world_x);
        const float world_z = interpolate(vertex[0].world_z, vertex[1].world_z,
                                          vertex[2].world_z);
        ++report.water_queries;
        if (!water.query(world_x, world_z, &water_bit)) {
          surface.water_query_failed = true;
          continue;
        }
      }
      color = water_bit
                  ? request.water_color
                  : sample_texture(texture, u, v, request.sampler_address);
      // Transparent-zero rejection is explicit and reported as unqualified;
      // no alpha threshold, blending equation or mip policy is implied.
      if ((color >> 24) == 0)
        continue;
      surface.color[pixel] = color;
      surface.depth[pixel] = depth;
      ++writes;
      if (domain == RasterDomain::City) {
        ++report.city_fragment_writes;
      } else if (water_bit) {
        ++report.water_fragment_writes;
      } else {
        ++report.terrain_fragment_writes;
      }
    }
  }
  return writes;
}

Vec3 bounds_center(const NdxrBounds &bounds,
                   const Mission01MapDrawInstance &draw) noexcept {
  return {(bounds.min_x + bounds.max_x) * 0.5F + draw.world_x,
          (bounds.min_y + bounds.max_y) * 0.5F + draw.world_y,
          (bounds.min_z + bounds.max_z) * 0.5F + draw.world_z};
}

float bounds_radius(const NdxrBounds &bounds) noexcept {
  const float x = (bounds.max_x - bounds.min_x) * 0.5F;
  const float y = (bounds.max_y - bounds.min_y) * 0.5F;
  const float z = (bounds.max_z - bounds.min_z) * 0.5F;
  return std::sqrt(x * x + y * y + z * z);
}

bool render_terrain(
    CpuSurface &surface, const CameraProjection &camera,
    const Mission01CpuFrameRequest &request,
    const RetailMission01MapRenderAssets &assets,
    const std::function<const DecodedTexture *(std::uint8_t)> &texture_for,
    Mission01CpuFrameReport &report) {
  const Mission01TerrainRenderResource &terrain = assets.terrain_resource();
  const Mission01WaterRenderResource &water = assets.water_resource();
  for (std::size_t instance_index = 0;
       instance_index < terrain.draw_instances.size(); ++instance_index) {
    ++report.terrain_instances_considered;
    std::array<Mission01TerrainResolvedVertex, 40> resolved;
    bool complete = true;
    Vec3 minimum{std::numeric_limits<float>::infinity(),
                 std::numeric_limits<float>::infinity(),
                 std::numeric_limits<float>::infinity()};
    Vec3 maximum{-std::numeric_limits<float>::infinity(),
                 -std::numeric_limits<float>::infinity(),
                 -std::numeric_limits<float>::infinity()};
    for (std::size_t index = 0; index < resolved.size(); ++index) {
      const std::optional<Mission01TerrainResolvedVertex> vertex =
          terrain.resolve_vertex(instance_index, index);
      if (!vertex.has_value()) {
        complete = false;
        break;
      }
      resolved[index] = *vertex;
      minimum.x = std::min(minimum.x, vertex->world[0]);
      minimum.y = std::min(minimum.y, vertex->world[1]);
      minimum.z = std::min(minimum.z, vertex->world[2]);
      maximum.x = std::max(maximum.x, vertex->world[0]);
      maximum.y = std::max(maximum.y, vertex->world[1]);
      maximum.z = std::max(maximum.z, vertex->world[2]);
    }
    if (!complete)
      continue;
    const Vec3 center{(minimum.x + maximum.x) * 0.5F,
                      (minimum.y + maximum.y) * 0.5F,
                      (minimum.z + maximum.z) * 0.5F};
    const Vec3 half = maximum - center;
    const float radius = std::sqrt(dot(half, half));
    if (!camera.sphere_visible(center, radius))
      continue;
    ++report.terrain_instances_visible;
    const std::uint8_t page = terrain.draw_instances[instance_index].atlas.page;
    const DecodedTexture *texture = texture_for(page);
    if (texture == nullptr)
      return false;
    std::size_t instance_writes = 0;
    for (std::size_t fan = 0; fan < 4; ++fan) {
      const std::size_t first = fan * 11;
      const std::uint16_t center_index = terrain.topology.fan_indices[first];
      if (center_index >= resolved.size() ||
          terrain.topology.fan_indices[first + 10] !=
              kMission01TerrainRestartIndex) {
        return false;
      }
      for (std::size_t edge_index = 1; edge_index < 9; ++edge_index) {
        const std::uint16_t left =
            terrain.topology.fan_indices[first + edge_index];
        const std::uint16_t right =
            terrain.topology.fan_indices[first + edge_index + 1];
        if (left >= resolved.size() || right >= resolved.size())
          return false;
        const auto make_vertex = [&](std::uint16_t index) noexcept {
          return WorldVertex{{resolved[index].world[0],
                              resolved[index].world[1],
                              resolved[index].world[2]},
                             resolved[index].uv[0],
                             resolved[index].uv[1]};
        };
        const std::array<WorldVertex, 3> triangle{
            make_vertex(center_index), make_vertex(left), make_vertex(right)};
        ++report.terrain_candidate_triangles;
        const std::size_t writes =
            rasterize_triangle(surface, camera, triangle, *texture,
                               RasterDomain::Terrain, request, water, report);
        if (writes != 0)
          ++report.terrain_rasterized_triangles;
        instance_writes += writes;
      }
    }
    if (instance_writes != 0)
      ++report.terrain_instances_rasterized;
  }
  return !surface.water_query_failed;
}

bool render_city(
    CpuSurface &surface, const CameraProjection &camera,
    const Mission01CpuFrameRequest &request,
    const RetailMission01MapRenderAssets &assets,
    const std::function<const DecodedTexture *(std::uint32_t)> &texture_for,
    Mission01CpuFrameReport &report) {
  const Mission01WaterRenderResource &water = assets.water_resource();
  for (const Mission01MapDrawInstance &draw : assets.draw_instances()) {
    ++report.city_instances_considered;
    const Mission01MapPrimitive *primitive = assets.primitive_for(draw);
    if (primitive == nullptr || !primitive->geometry.bounds.valid)
      return false;
    if (!camera.sphere_visible(bounds_center(primitive->geometry.bounds, draw),
                               bounds_radius(primitive->geometry.bounds))) {
      continue;
    }
    ++report.city_instances_visible;
    const DecodedTexture *texture = texture_for(primitive->texture_identifier);
    if (texture == nullptr)
      return false;
    const NdxrMesh &geometry = primitive->geometry;
    std::array<std::uint16_t, 3> strip{};
    std::size_t strip_count = 0;
    std::size_t instance_writes = 0;
    for (const std::uint16_t index : geometry.indices) {
      if (index == kStripRestart) {
        strip_count = 0;
        continue;
      }
      if (index >= geometry.positions.size() ||
          index >= geometry.texcoords.size()) {
        return false;
      }
      strip[strip_count % 3] = index;
      ++strip_count;
      if (strip_count < 3)
        continue;
      std::uint16_t a = strip[(strip_count - 3) % 3];
      std::uint16_t b = strip[(strip_count - 2) % 3];
      const std::uint16_t c = strip[(strip_count - 1) % 3];
      if ((strip_count & 1u) == 0u)
        std::swap(a, b);
      const auto make_vertex = [&](std::uint16_t vertex_index) noexcept {
        const NdxrPosition &position = geometry.positions[vertex_index];
        const NdxrTexcoord &uv = geometry.texcoords[vertex_index];
        return WorldVertex{{draw.world_x + position.x,
                            draw.world_y + position.y,
                            draw.world_z + position.z},
                           uv.u,
                           uv.v};
      };
      const std::array<WorldVertex, 3> triangle{make_vertex(a), make_vertex(b),
                                                make_vertex(c)};
      ++report.city_candidate_triangles;
      const std::size_t writes =
          rasterize_triangle(surface, camera, triangle, *texture,
                             RasterDomain::City, request, water, report);
      if (writes != 0)
        ++report.city_rasterized_triangles;
      instance_writes += writes;
    }
    if (instance_writes != 0)
      ++report.city_instances_rasterized;
  }
  return true;
}

std::uint64_t fnv_append(std::uint64_t hash, std::uint32_t value) noexcept {
  for (unsigned byte = 0; byte < 4; ++byte) {
    hash ^= static_cast<std::uint8_t>(value >> (byte * 8));
    hash *= 1099511628211ULL;
  }
  return hash;
}

void finalize_report(const CpuSurface &surface,
                     Mission01CpuFrameReport &report) noexcept {
  report.color_hash = 1469598103934665603ULL;
  report.depth_hash = 1469598103934665603ULL;
  for (std::size_t index = 0; index < surface.color.size(); ++index) {
    report.color_hash = fnv_append(report.color_hash, surface.color[index]);
    if (surface.color[index] != surface.clear_color)
      ++report.color_coverage;
    std::uint32_t quantized = std::numeric_limits<std::uint32_t>::max();
    if (std::isfinite(surface.depth[index])) {
      ++report.depth_coverage;
      quantized = static_cast<std::uint32_t>(
          std::clamp(surface.depth[index] / kFarPlane, 0.0F, 1.0F) *
          16777215.0F);
    }
    report.depth_hash = fnv_append(report.depth_hash, quantized);
  }
}

bool digest_nonzero(const Sha256Digest &digest) noexcept {
  return std::any_of(digest.begin(), digest.end(),
                     [](std::uint8_t value) { return value != 0; });
}

} // namespace

bool Mission01CpuFrameReport::jv_eligible() const noexcept {
  return marker_free() && store_backed && terrain_geometry_retail &&
         terrain_uv_retail && water_mask_retail && city_geometry_retail &&
         city_binding_retail && city_transform_retail && camera_group_retail &&
         camera_fov_retail && camera_mode_selection_retail &&
         camera_mode2_base_transform_retail && camera_dynamic_offset_retail &&
         camera_runtime_state_retail && camera_pose_retail &&
         clip_pipeline_retail && map_distance_policy_retail &&
         texture_byte_swap_retail && mip_policy_retail &&
         sampler_state_retail && alpha_state_retail && water_material_retail &&
         sky_retail && vegetation_retail && active_units_retail &&
         depth_coverage != 0;
}

bool Mission01CpuFrame::write_ppm(
    const std::filesystem::path &path) const noexcept {
  if (path.empty() || report_.width == 0 || report_.height == 0 ||
      pixels_.size() !=
          static_cast<std::size_t>(report_.width) * report_.height) {
    return false;
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output)
    return false;
  output << "P6\n" << report_.width << ' ' << report_.height << "\n255\n";
  for (const std::uint32_t pixel : pixels_) {
    const unsigned char rgb[3]{static_cast<unsigned char>(pixel >> 16),
                               static_cast<unsigned char>(pixel >> 8),
                               static_cast<unsigned char>(pixel)};
    output.write(reinterpret_cast<const char *>(rgb), sizeof(rgb));
  }
  return static_cast<bool>(output);
}

bool Mission01CpuFrame::write_report_json(
    const std::filesystem::path &path) const noexcept {
  if (path.empty())
    return false;
  std::ofstream output(path, std::ios::trunc);
  if (!output)
    return false;
  const auto write_float4 = [&output](const std::array<float, 4> &values) {
    output << '[' << values[0] << ", " << values[1] << ", " << values[2]
           << ", " << values[3] << ']';
  };
  output << "{\n"
         << "  \"schema\": \"ac6.mission01-cpu-frame.v3\",\n"
         << "  \"content_index_sha256\": \""
         << sha256_hex(report_.content_index_sha256) << "\",\n"
         << "  \"width\": " << report_.width << ",\n"
         << "  \"height\": " << report_.height << ",\n"
         << "  \"camera_group\": " << report_.camera_group << ",\n"
         << "  \"view_mode\": " << report_.view_mode << ",\n"
         << "  \"alternate_fov\": "
         << (report_.alternate_fov ? "true" : "false") << ",\n"
         << "  \"fov_radians\": " << std::setprecision(9) << report_.fov_radians
         << ",\n"
         << "  \"near_plane\": " << report_.near_plane << ",\n"
         << "  \"far_plane\": " << report_.far_plane << ",\n"
         << "  \"camera_source\": \""
         << (report_.uses_external_camera_pose
                 ? "external_pose"
                 : (report_.camera_dynamic_offset_retail
                        ? "retail_mode2_dynamic"
                        : "retail_mode2_base"))
         << "\",\n"
         << "  \"camera_eye\": [" << report_.camera_pose.eye[0] << ", "
         << report_.camera_pose.eye[1] << ", " << report_.camera_pose.eye[2]
         << "],\n"
         << "  \"camera_target\": [" << report_.camera_pose.target[0] << ", "
         << report_.camera_pose.target[1] << ", "
         << report_.camera_pose.target[2] << "],\n"
         << "  \"camera_up\": [" << report_.camera_pose.up[0] << ", "
         << report_.camera_pose.up[1] << ", " << report_.camera_pose.up[2]
         << "],\n"
         << "  \"texture_swap_16\": "
         << (report_.texture_swap_16 ? "true" : "false") << ",\n"
         << "  \"sampler_address\": \""
         << (report_.sampler_address == Mission01CpuSamplerAddress::Repeat
                 ? "repeat"
                 : "clamp")
         << "\",\n"
         << "  \"clear_color\": \"0x" << std::hex << std::setw(8)
         << std::setfill('0') << report_.clear_color << "\",\n"
         << "  \"water_color\": \"0x" << std::setw(8) << report_.water_color
         << std::dec << std::setfill(' ') << "\",\n"
         << "  \"terrain_instances_considered\": "
         << report_.terrain_instances_considered << ",\n"
         << "  \"terrain_instances_visible\": "
         << report_.terrain_instances_visible << ",\n"
         << "  \"terrain_instances_rasterized\": "
         << report_.terrain_instances_rasterized << ",\n"
         << "  \"terrain_candidate_triangles\": "
         << report_.terrain_candidate_triangles << ",\n"
         << "  \"terrain_triangles\": " << report_.terrain_rasterized_triangles
         << ",\n"
         << "  \"city_instances_considered\": "
         << report_.city_instances_considered << ",\n"
         << "  \"city_instances_visible\": " << report_.city_instances_visible
         << ",\n"
         << "  \"city_instances_rasterized\": "
         << report_.city_instances_rasterized << ",\n"
         << "  \"city_candidate_triangles\": "
         << report_.city_candidate_triangles << ",\n"
         << "  \"city_triangles\": " << report_.city_rasterized_triangles
         << ",\n"
         << "  \"terrain_fragment_writes\": " << report_.terrain_fragment_writes
         << ",\n"
         << "  \"water_fragment_writes\": " << report_.water_fragment_writes
         << ",\n"
         << "  \"city_fragment_writes\": " << report_.city_fragment_writes
         << ",\n"
         << "  \"water_queries\": " << report_.water_queries << ",\n"
         << "  \"color_coverage\": " << report_.color_coverage << ",\n"
         << "  \"depth_coverage\": " << report_.depth_coverage << ",\n"
         << "  \"color_hash\": \"0x" << std::hex << report_.color_hash
         << "\",\n"
         << "  \"depth_hash\": \"0x" << report_.depth_hash << std::dec
         << "\",\n"
         << "  \"marker_writes\": " << report_.marker_writes << ",\n"
         << "  \"camera_mode2_base_transform_retail\": "
         << (report_.camera_mode2_base_transform_retail ? "true" : "false")
         << ",\n"
         << "  \"camera_dynamic_offset_retail\": "
         << (report_.camera_dynamic_offset_retail ? "true" : "false") << ",\n"
         << "  \"camera_dynamic_branch\": ";
  if (report_.camera_dynamic_offset_retail) {
    output << '"' << mode2_dynamic_branch_name(report_.camera_dynamic_branch)
           << '"';
  } else {
    output << "null";
  }
  output << ",\n"
         << "  \"camera_random_draws_consumed\": "
         << static_cast<unsigned>(report_.camera_random_draws_consumed)
         << ",\n"
         << "  \"camera_next_shake_state\": ";
  if (report_.next_mode2_shake_state.has_value()) {
    const RetailMode2ShakeState &state = *report_.next_mode2_shake_state;
    output << "{\"start\": ";
    write_float4(state.start);
    output << ", \"target\": ";
    write_float4(state.target);
    output << ", \"output\": ";
    write_float4(state.output);
    output << ", \"velocity\": ";
    write_float4(state.velocity);
    output << ", \"elapsed\": " << state.elapsed << '}';
  } else {
    output << "null";
  }
  output << ",\n"
         << "  \"camera_runtime_state_retail\": "
         << (report_.camera_runtime_state_retail ? "true" : "false") << ",\n"
         << "  \"decoded_atlas_pages\": [";
  for (std::size_t index = 0; index < report_.decoded_atlas_pages.size();
       ++index) {
    if (index != 0)
      output << ", ";
    output << static_cast<unsigned>(report_.decoded_atlas_pages[index]);
  }
  output << "],\n  \"decoded_map_texture_ids\": [";
  for (std::size_t index = 0; index < report_.decoded_map_texture_ids.size();
       ++index) {
    if (index != 0)
      output << ", ";
    output << report_.decoded_map_texture_ids[index];
  }
  output << "],\n"
         << "  \"marker_free\": " << (report_.marker_free() ? "true" : "false")
         << ",\n"
         << "  \"jv_eligible\": " << (report_.jv_eligible() ? "true" : "false")
         << ",\n"
         << "  \"closed_domains\": [\"terrain_geometry\", "
            "\"terrain_uv\", \"water_mask\", \"city_geometry\", "
            "\"city_binding\", \"city_transform\", \"camera_group\", "
            "\"camera_fov\"";
  if (report_.camera_mode2_base_transform_retail) {
    output << ", \"camera_mode2_base_transform\"";
  }
  if (report_.camera_dynamic_offset_retail) {
    output << ", \"camera_dynamic_offset\"";
  }
  output << "],\n"
         << "  \"open_boundaries\": [\"camera_mode_selection\"";
  if (!report_.camera_dynamic_offset_retail) {
    output << ", \"camera_dynamic_offset\"";
  }
  output << ", \"camera_runtime_state\", \"camera_pose\", "
            "\"clip_pipeline\", \"map_distance_policy\", "
            "\"texture_byte_swap\", \"mip_policy\", \"sampler_state\", "
            "\"alpha_state\", \"water_material\", \"sky\", "
            "\"vegetation\", \"active_units\"]\n"
         << "}\n";
  return static_cast<bool>(output);
}

RetailMission01CpuCompositor::RetailMission01CpuCompositor(
    RetailMission01MapRenderAssets assets, RetailCameraTable camera_table)
    : assets_(std::move(assets)), camera_table_(std::move(camera_table)) {}

std::optional<RetailMission01CpuCompositor>
RetailMission01CpuCompositor::open(const RetailContentStore &store) {
  std::optional<RetailMission01MapRenderAssets> assets =
      RetailMission01MapRenderAssets::open(store);
  const std::optional<RetailCampaignBundle> common =
      RetailCampaignBundle::open_entry(store, kRetailCameraTableEntry);
  const std::optional<RetailCameraTable> camera_table =
      common.has_value() ? RetailCameraTable::open(*common) : std::nullopt;
  if (!assets.has_value() || !common.has_value() || !camera_table.has_value()) {
    return std::nullopt;
  }
  return assemble(std::move(*assets), *camera_table,
                  common->content_index_sha256());
}

std::optional<RetailMission01CpuCompositor>
RetailMission01CpuCompositor::assemble(
    RetailMission01MapRenderAssets assets, RetailCameraTable camera_table,
    const Sha256Digest &camera_content_index_sha256) {
  if (!assets.store_backed() || !assets.report().complete ||
      !digest_nonzero(camera_content_index_sha256) ||
      assets.content_index_sha256() != camera_content_index_sha256) {
    return std::nullopt;
  }
  return RetailMission01CpuCompositor(std::move(assets),
                                      std::move(camera_table));
}

const DecodedTexture *
RetailMission01CpuCompositor::map_texture(std::uint32_t identifier,
                                          bool swap_16) {
  const std::uint64_t key =
      (static_cast<std::uint64_t>(identifier) << 1) | (swap_16 ? 1u : 0u);
  auto found = map_textures_.find(key);
  if (found == map_textures_.end()) {
    DecodedTexture decoded;
    if (std::optional<DecodedTexture> texture =
            assets_.decode_texture(identifier, swap_16)) {
      decoded = std::move(*texture);
    }
    found = map_textures_.emplace(key, std::move(decoded)).first;
  }
  return found->second.pixels.empty() ? nullptr : &found->second;
}

const DecodedTexture *
RetailMission01CpuCompositor::atlas_texture(std::uint8_t page, bool swap_16) {
  const std::uint64_t key =
      (static_cast<std::uint64_t>(page) << 1) | (swap_16 ? 1u : 0u);
  auto found = atlas_textures_.find(key);
  if (found == atlas_textures_.end()) {
    DecodedTexture decoded;
    const std::vector<Mission01TerrainAtlasPage> &pages =
        assets_.terrain_resource().atlas_pages;
    if (page < pages.size() && pages[page].page == page) {
      NtxrRefusal refusal = NtxrRefusal::BadHeader;
      if (std::optional<DecodedTexture> texture = decode_ntxr_base_level(
              pages[page].source.data(), pages[page].source.size(), swap_16,
              &refusal)) {
        decoded = std::move(*texture);
      }
    }
    found = atlas_textures_.emplace(key, std::move(decoded)).first;
  }
  return found->second.pixels.empty() ? nullptr : &found->second;
}

std::optional<Mission01CpuFrame>
RetailMission01CpuCompositor::render(const Mission01CpuFrameRequest &request) {
  const std::optional<std::uint32_t> group =
      RetailCameraTable::group_for_loadout(request.loadout);
  const RetailCameraRecord *camera_record =
      camera_table_.record_for_loadout(request.loadout, request.view_mode);
  if (!group.has_value() || camera_record == nullptr ||
      (request.sampler_address != Mission01CpuSamplerAddress::Clamp &&
       request.sampler_address != Mission01CpuSamplerAddress::Repeat) ||
      (request.clear_color >> 24) != 0xFFu ||
      (request.water_color >> 24) != 0xFFu) {
    return std::nullopt;
  }
  const float fov = request.alternate_fov
                        ? camera_record->alternate_fov_radians()
                        : camera_record->fov_radians();
  std::optional<RetailMode2CameraLocator> mode2_locator;
  std::optional<RetailMode2DynamicResult> mode2_dynamic;
  if (request.mode2_dynamic_input.has_value() &&
      !request.mode2_camera_state.has_value()) {
    return std::nullopt;
  }
  if (request.mode2_camera_state.has_value()) {
    if (request.view_mode != 2)
      return std::nullopt;
    const std::optional<std::array<float, 4>> base_offset =
        camera_record->offset(0);
    if (!base_offset.has_value())
      return std::nullopt;
    std::array<float, 4> local_offset = *base_offset;
    if (request.mode2_dynamic_input.has_value()) {
      mode2_dynamic = apply_mode2_dynamic_offset(
          local_offset, *request.mode2_dynamic_input);
      if (!mode2_dynamic.has_value())
        return std::nullopt;
      local_offset = mode2_dynamic->local_offset;
    }
    mode2_locator = transform_mode2_camera_locator(
        *request.mode2_camera_state, local_offset);
    if (!mode2_locator.has_value())
      return std::nullopt;
  }
  const std::optional<CameraProjection> camera =
      mode2_locator.has_value()
          ? CameraProjection::make_mode2(request, fov, *mode2_locator)
          : CameraProjection::make_external(request, fov);
  if (!camera.has_value())
    return std::nullopt;

  CpuSurface surface;
  surface.width = request.width;
  surface.height = request.height;
  surface.clear_color = request.clear_color;
  const std::size_t pixels =
      static_cast<std::size_t>(request.width) * request.height;
  surface.color.assign(pixels, request.clear_color);
  surface.depth.assign(pixels, std::numeric_limits<float>::infinity());

  Mission01CpuFrameReport report;
  report.content_index_sha256 = assets_.content_index_sha256();
  report.width = request.width;
  report.height = request.height;
  report.camera_group = *group;
  report.view_mode = request.view_mode;
  report.alternate_fov = request.alternate_fov;
  report.fov_radians = fov;
  report.near_plane = kNearPlane;
  report.far_plane = kFarPlane;
  report.uses_external_camera_pose = !mode2_locator.has_value();
  if (mode2_locator.has_value()) {
    report.camera_pose.eye = mode2_locator->position;
    for (std::size_t lane = 0; lane < 3; ++lane) {
      report.camera_pose.target[lane] =
          mode2_locator->position[lane] + mode2_locator->basis.rows[2][lane];
      report.camera_pose.up[lane] = mode2_locator->basis.rows[1][lane];
    }
  } else {
    report.camera_pose = request.pose;
  }
  report.texture_swap_16 = request.texture_swap_16;
  report.sampler_address = request.sampler_address;
  report.clear_color = request.clear_color;
  report.water_color = request.water_color;
  report.store_backed = assets_.store_backed();
  report.uses_manifest_tsv = false;
  report.terrain_geometry_retail = true;
  report.terrain_uv_retail = true;
  report.water_mask_retail = true;
  report.city_geometry_retail = true;
  report.city_binding_retail = true;
  report.city_transform_retail = true;
  report.camera_group_retail = true;
  report.camera_fov_retail = true;
  report.camera_mode2_base_transform_retail = mode2_locator.has_value();
  report.camera_dynamic_offset_retail = mode2_dynamic.has_value();
  if (mode2_dynamic.has_value()) {
    report.camera_dynamic_branch = mode2_dynamic->branch;
    report.camera_random_draws_consumed =
        mode2_dynamic->random_draws_consumed;
    report.next_mode2_shake_state = mode2_dynamic->shake;
  }

  std::set<std::uint8_t> atlas_pages;
  std::set<std::uint32_t> map_textures;
  const auto terrain_texture = [&](std::uint8_t page) {
    const DecodedTexture *texture =
        atlas_texture(page, request.texture_swap_16);
    if (texture != nullptr)
      atlas_pages.insert(page);
    return texture;
  };
  const auto city_texture = [&](std::uint32_t identifier) {
    const DecodedTexture *texture =
        map_texture(identifier, request.texture_swap_16);
    if (texture != nullptr)
      map_textures.insert(identifier);
    return texture;
  };
  if (!render_terrain(surface, *camera, request, assets_, terrain_texture,
                      report) ||
      !render_city(surface, *camera, request, assets_, city_texture, report)) {
    return std::nullopt;
  }
  report.decoded_atlas_pages.assign(atlas_pages.begin(), atlas_pages.end());
  report.decoded_map_texture_ids.assign(map_textures.begin(),
                                        map_textures.end());
  finalize_report(surface, report);

  Mission01CpuFrame frame;
  frame.pixels_ = std::move(surface.color);
  frame.depth_ = std::move(surface.depth);
  frame.report_ = std::move(report);
  return frame;
}

} // namespace ac6::retail
