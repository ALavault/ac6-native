#include "ac6/product_runtime.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

namespace ac6 {
namespace {

struct Vec3 {
  float x{};
  float y{};
  float z{};
};

std::uint32_t stable_hash32(std::string_view text) noexcept {
  std::uint32_t hash = 2166136261u;
  for (const char byte : text) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= 16777619u;
  }
  return hash;
}

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
  screen.x = std::min(
      width - 1u,
      static_cast<std::uint32_t>((safe_x * 0.5f + 0.5f) * static_cast<float>(width)));
  screen.y = std::min(
      height - 1u,
      static_cast<std::uint32_t>((0.5f - safe_y * 0.5f) * static_cast<float>(height)));
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
                   ScreenPoint& screen, bool clip_to_viewport = true,
                   float far_plane_override = 0.0f) noexcept {
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
  //
  // A caller that knows its content is further away than that says so, because
  // the clamp is not harmless: every point beyond the far plane normalises to
  // exactly 1.0, which is the value the target is cleared to, and a depth test
  // written `depth >= stored` then rejects all of them. Cycle 1146 found the
  // whole retail world disappearing that way - Mission 01 spans 66,000 units
  // and this plane is 4,096 - and the effect looked exactly like a camera
  // pointing the wrong way, which is what cycle 1145 wrongly called it.
  //
  // The default is retail's own, derived in cycle 1273 rather than invented.
  // ACE6::CAce6CameraManager lives at [[0x826E4EB4] + 0x2F9A0] (RTTI at
  // 0x8268F47C, vtable 0x82054F0C) and its initialiser 0x8225DF88 stamps the
  // four projection fields:
  //
  //   8225dfb0  lfs  f0,0x7f64(r11)    [0x82007F64] = 0.8028514 rad = 46.0000 deg
  //   8225dfb8  stfs f0,0xd4(r31)      +0xD4 vertical FOV
  //   8225dfc8  stfs f0,0xd8(r31)      +0xD8 aspect = 1.7777778 = 16/9
  //   8225dfe4  stfs f30,0xdc(r31)     +0xDC near  = 1.0
  //   8225e000  stfs f0,0xe0(r31)      +0xE0 far   = 24000.0
  //
  // and 0x8209B398 hands exactly those four to the perspective builder:
  //
  //   8209b42c  lfs f4,0xe0(r31)   8209b430  lfs f3,0xdc(r31)
  //   8209b434  lfs f2,0xd8(r31)   8209b438  lfs f1,0xd4(r31)
  //   8209b43c  bl 0x82271828
  //
  // WHAT IS IMPORTED IS THE NUMBER, NOT THE PROJECTION. Retail's builder
  // 0x82339DB0 is right-handed (m23 = -1.0 read at [0x82069B28]) with depth
  // running 1 at the near plane to 0 at the far one; this rasteriser keeps its
  // own linear `view_z / far`. The 46-degree vertical FOV is also not applied
  // here, and it is retail's INITIAL value in any case -- the per-frame camera
  // behaviour overwrites +0xD4 every tick through the virtual at
  // ArmsCamera+0x850, and that behaviour is not derived.
  constexpr float default_far_plane = 24000.0f;
  const float far_plane =
      (far_plane_override > 0.0f && std::isfinite(far_plane_override))
          ? far_plane_override
          : default_far_plane;
  screen.depth = std::clamp(view_z / far_plane, 0.0f, 1.0f);
  screen.ndc_x = ndc_x;
  screen.ndc_y = ndc_y;
  screen.ndc_z = screen.depth * 2.0f - 1.0f;
  return true;
}


}  // namespace

struct NativeRenderTarget::ClipVertex {
  float x{};
  float y{};
  float z{};
  float u{};
  float v{};
};

struct NativeRenderTarget::GeometryRasterContext {
  const WorldFrame* frame{};
  const MissionDrawable* drawable{};
  const NativeGeometryMetadata* geometry{};
  const DecodedGeometry* decoded{};
  const MissionDrawableTransform* transform{};
  const MissionMaterial* material{};
  const MissionTextureBinding* texture{};
  const ShaderPermutation* shader{};
  const MissionRenderTargetDefinition* render_target{};
  const MissionRenderTargetDefinition* destination_target{};
  const MissionRenderPass* pass{};
  const MissionRenderResolve* resolve{};
  const MissionCameraDefinition* camera{};
  const MissionTextureDatabase* texture_database{};
  std::uint32_t ordinal{};
  NativeCameraProjection projection{};
  std::vector<ScreenPoint> projected_vertices;
  std::vector<std::uint8_t> projected_valid;
  std::uint32_t texture_salt{};
  std::uint32_t shader_salt{};
  std::uint32_t material_salt{};
  NativeRasterDrawableMetrics* metrics{};
};

bool NativeRenderTarget::resize(std::uint32_t width, std::uint32_t height) {
  constexpr std::uint32_t max_dimension = 4096;
  if (width == 0 || height == 0 || width > max_dimension || height > max_dimension) {
    return false;
  }
  const std::uint64_t pixels = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
  if (pixels > 16u * 1024u * 1024u) return false;
  width_ = width;
  height_ = height;
  color_.assign(static_cast<std::size_t>(pixels), 0);
  depth_.assign(static_cast<std::size_t>(pixels), 1.0f);
  object_ids_.assign(static_cast<std::size_t>(pixels), 0);
  pixel_stamps_.assign(static_cast<std::size_t>(pixels), 0);
  hud_pixel_stamps_.assign(static_cast<std::size_t>(pixels), 0);
  geometry_calls_ = 0;
  raster_triangles_ = 0;
  raster_writes_ = 0;
  diagnostic_point_writes_ = 0;
  filled_fragment_writes_ = 0;
  raster_stamp_ = 0;
  hud_stamp_ = 0;
  hud_pixel_writes_ = 0;
  hud_unique_pixels_ = 0;
  raster_metrics_.clear();
  return true;
}

bool NativeRenderTarget::clear(std::uint32_t color, float depth) {
  if (width_ == 0 || height_ == 0 || color_.empty() || depth_.empty() || !std::isfinite(depth)) {
    return false;
  }
  std::fill(color_.begin(), color_.end(), color);
  std::fill(depth_.begin(), depth_.end(), depth);
  std::fill(object_ids_.begin(), object_ids_.end(), 0);
  std::fill(pixel_stamps_.begin(), pixel_stamps_.end(), 0);
  std::fill(hud_pixel_stamps_.begin(), hud_pixel_stamps_.end(), 0);
  geometry_calls_ = 0;
  raster_triangles_ = 0;
  raster_writes_ = 0;
  diagnostic_point_writes_ = 0;
  filled_fragment_writes_ = 0;
  raster_stamp_ = 0;
  hud_stamp_ = 0;
  hud_pixel_writes_ = 0;
  hud_unique_pixels_ = 0;
  raster_metrics_.clear();
  return true;
}

bool NativeRenderTarget::draw_hud_rect(std::uint32_t min_x, std::uint32_t min_y,
                                       std::uint32_t max_x, std::uint32_t max_y,
                                       std::uint32_t color) noexcept {
  if (width_ == 0 || height_ == 0 || color_.empty() || color == 0 ||
      min_x > max_x || min_y > max_y) return false;
  if (min_x >= width_ || min_y >= height_) return true;
  max_x = std::min(max_x, width_ - 1u);
  max_y = std::min(max_y, height_ - 1u);
  if (++hud_stamp_ == 0) {
    std::fill(hud_pixel_stamps_.begin(), hud_pixel_stamps_.end(), 0);
    hud_stamp_ = 1;
  }
  for (std::uint32_t y = min_y; y <= max_y; ++y) {
    for (std::uint32_t x = min_x; x <= max_x; ++x) {
      const std::size_t index = static_cast<std::size_t>(y) * width_ + x;
      color_[index] = color;
      ++hud_pixel_writes_;
      if (hud_pixel_stamps_[index] != hud_stamp_) {
        hud_pixel_stamps_[index] = hud_stamp_;
        ++hud_unique_pixels_;
      }
    }
  }
  return true;
}

bool NativeRenderTarget::mark_world_asset(const WorldFrame& frame, AssetId asset,
                                          std::uint32_t ordinal) noexcept {
  if (raster_mode_ != NativeRasterMode::Points) return false;
  if (width_ == 0 || height_ == 0 || color_.empty() || depth_.empty() || asset == 0 ||
      !frame.mission_ready) {
    return false;
  }
  const std::uint64_t seed = static_cast<std::uint64_t>(asset) * 2654435761ull +
                             static_cast<std::uint64_t>(frame.mission_id) * 2246822519ull +
                             static_cast<std::uint64_t>(ordinal) * 3266489917ull;
  const std::uint32_t x = static_cast<std::uint32_t>(seed % width_);
  const std::uint32_t y = static_cast<std::uint32_t>((seed / width_) % height_);
  const std::size_t index = static_cast<std::size_t>(y) * width_ + x;
  color_[index] = 0xFF000000u | ((asset & 0xFFu) << 16) |
                  ((frame.mission_id & 0xFFu) << 8) | (ordinal & 0xFFu);
  depth_[index] = 1.0f / static_cast<float>(ordinal + 2u);
  ++diagnostic_point_writes_;
  return true;
}

// Diagnostic: a world position, projected by the same two camera paths the
// geometry raster uses - the qualified c218-c221 rows when a camera is given,
// the runtime's own basis otherwise - and plotted depth-tested. No material, no
// texture, no topology; see the header for why this lane exists and what it may
// not be used to claim.
bool NativeRenderTarget::draw_world_marker(const WorldFrame& frame,
                                           const CombatVector& position,
                                           std::uint32_t color, std::uint32_t radius,
                                           const MissionCameraDefinition* camera,
                                           float far_plane) noexcept {
  if (width_ == 0 || height_ == 0 || color_.empty() || depth_.empty()) return false;
  if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
      !std::isfinite(position.z)) {
    return false;
  }
  const Vec3 world{position.x, position.y, position.z};
  ScreenPoint screen;
  if (camera != nullptr) {
    if (!project_clip_point(*camera, world, width_, height_, screen)) return false;
  } else {
    NativeCameraProjection projection;
    if (!make_projection(frame, width_, height_, projection)) return false;
    if (!project_point(projection, world, width_, height_, screen, true, far_plane)) {
      return false;
    }
  }
  const float depth_value = std::clamp(screen.depth, 0.0f, 1.0f);
  const std::uint32_t span = std::min(radius, 64u);
  bool wrote = false;
  const std::uint32_t min_x = screen.x > span ? screen.x - span : 0u;
  const std::uint32_t min_y = screen.y > span ? screen.y - span : 0u;
  const std::uint32_t max_x = std::min(width_ - 1u, screen.x + span);
  const std::uint32_t max_y = std::min(height_ - 1u, screen.y + span);
  for (std::uint32_t y = min_y; y <= max_y; ++y) {
    for (std::uint32_t x = min_x; x <= max_x; ++x) {
      const std::size_t pixel = static_cast<std::size_t>(y) * width_ + x;
      if (depth_value >= depth_[pixel]) continue;
      depth_[pixel] = depth_value;
      color_[pixel] = color;
      ++world_marker_writes_;
      wrote = true;
    }
  }
  return wrote;
}

bool NativeRenderTarget::draw_world_asset(const WorldFrame& frame,
                                          const MissionDrawable& drawable,
                                          std::uint32_t ordinal) noexcept {
  if (raster_mode_ != NativeRasterMode::Points) return false;
  if (drawable.mission_id != frame.mission_id || drawable.stable_id.empty() ||
      drawable.kind.empty() || drawable.asset == 0 ||
      drawable.primitive_count == 0 || !drawable.has_buffer_contract()) {
    return false;
  }
  const std::uint32_t samples = std::min<std::uint32_t>(drawable.primitive_count, 64u);
  for (std::uint32_t sample = 0; sample < samples; ++sample) {
    if (!mark_world_asset(frame, drawable.asset, ordinal * 131u + sample)) return false;
  }
  return true;
}

bool NativeRenderTarget::prepare_geometry_draw(GeometryRasterContext& context) {
  const WorldFrame& frame = *context.frame;
  const MissionDrawable& drawable = *context.drawable;
  const NativeGeometryMetadata& geometry = *context.geometry;
  const DecodedGeometry& decoded = *context.decoded;
  const MissionDrawableTransform& transform = *context.transform;
  const MissionMaterial& material = *context.material;
  const MissionTextureBinding& texture = *context.texture;
  const ShaderPermutation& shader = *context.shader;
  const MissionRenderTargetDefinition& render_target = *context.render_target;
  const MissionRenderTargetDefinition& destination_target = *context.destination_target;
  const MissionRenderPass& pass = *context.pass;
  const MissionRenderResolve& resolve = *context.resolve;
  const bool msaa_resolve = resolve.mode == "msaa_resolve";
  const bool sample_contract_valid =
      msaa_resolve ? (render_target.sample_count > 1 && destination_target.sample_count == 1)
                   : (resolve.mode == "tonemap" ? destination_target.sample_count == 1
                                                : render_target.sample_count == destination_target.sample_count);
  if (width_ == 0 || height_ == 0 || color_.empty() || depth_.empty() ||
      !frame.mission_ready ||
      drawable.mission_id != frame.mission_id || drawable.stable_id.empty() ||
      drawable.kind.empty() || drawable.asset == 0 ||
      drawable.primitive_count == 0 || !drawable.has_buffer_contract() ||
      !transform.valid() || transform.mission_id != drawable.mission_id ||
      transform.stable_id != drawable.stable_id ||
      !material.valid() || material.mission_id != drawable.mission_id ||
      material.stable_id != drawable.stable_id ||
      !texture.valid() || texture.mission_id != drawable.mission_id ||
      texture.stable_id != drawable.stable_id ||
      !shader.valid() || shader.id != material.shader_permutation ||
      !render_target.valid() || render_target.mission_id != frame.mission_id ||
      render_target.target_id != pass.color_target ||
      render_target.width != width_ || render_target.height != height_ ||
      render_target.color_format != shader.render_target_format ||
      (render_target.depth_enabled && render_target.depth_format != "d24s8") ||
      (!render_target.depth_enabled && (material.depth_test || material.depth_write)) ||
      !destination_target.valid() || destination_target.mission_id != frame.mission_id ||
      destination_target.target_id != resolve.destination_target ||
      destination_target.width != width_ || destination_target.height != height_ ||
      destination_target.color_format != render_target.color_format ||
      destination_target.depth_enabled || destination_target.depth_format != "none" ||
      !sample_contract_valid ||
      !pass.valid() || pass.mission_id != frame.mission_id || pass.pass_id != "world" ||
      (render_target.depth_enabled ? pass.depth_target != "main_depth" : pass.depth_target != "none") ||
      !resolve.valid() || resolve.mission_id != frame.mission_id ||
      resolve.source_pass != pass.pass_id || resolve.source_target != pass.color_target ||
      resolve.destination_target != "present" ||
      geometry.buffer_id != drawable.buffer_id ||
      decoded.buffer_id != drawable.buffer_id ||
      geometry.vertex_count != drawable.vertex_count ||
      geometry.index_count != drawable.index_count ||
      geometry.primitive_count != drawable.primitive_count ||
      geometry.vertex_section_count != drawable.vertex_count ||
      geometry.index_section_count != drawable.index_count ||
      geometry.polygon_descriptor_count != drawable.primitive_count ||
      geometry.vertex_stride == 0 || geometry.index_size == 0 ||
      geometry.vertex_byte_size == 0 || geometry.index_byte_size == 0 ||
      decoded.vertices.empty() || decoded.indices.empty() || !decoded.bounds.valid ||
      !std::isfinite(decoded.bounds.min_x) || !std::isfinite(decoded.bounds.min_y) ||
      !std::isfinite(decoded.bounds.min_z) || !std::isfinite(decoded.bounds.max_x) ||
      !std::isfinite(decoded.bounds.max_y) || !std::isfinite(decoded.bounds.max_z) ||
      decoded.bounds.min_x > decoded.bounds.max_x ||
      decoded.bounds.min_y > decoded.bounds.max_y ||
      decoded.bounds.min_z > decoded.bounds.max_z) {
    return false;
  }

  raster_metrics_.emplace_back();
  context.metrics = &raster_metrics_.back();
  context.metrics->stable_id = drawable.stable_id;
  context.metrics->object_id = stable_hash32(drawable.stable_id);
  if (++raster_stamp_ == 0) {
    std::fill(pixel_stamps_.begin(), pixel_stamps_.end(), 0);
    raster_stamp_ = 1;
  }
  ++geometry_calls_;

  if (context.camera == nullptr && !make_projection(frame, width_, height_, context.projection)) {
    return false;
  }
  context.texture_salt =
      static_cast<std::uint32_t>(texture.content_hash) ^
      static_cast<std::uint32_t>(texture.content_hash >> 32u) ^
      static_cast<std::uint32_t>(texture.gidx) ^
      static_cast<std::uint32_t>(texture.gidx >> 32u) ^
      (texture.sampler_filter == "linear" ? 0x13579BDFu : 0x2468ACE0u) ^
      (texture.sampler_address == "wrap" ? 0x10203040u : 0x50607080u);
  context.shader_salt =
      stable_hash32(shader.id) ^ stable_hash32(shader.vertex_layout) ^
      (shader.texture_fetches * 0x01010101u) ^
      (shader.constant_count * 0x00010001u) ^
      stable_hash32(shader.render_target_format) ^
      stable_hash32(pass.pass_id) ^ (pass.order * 0x11111111u) ^
      stable_hash32(pass.color_target) ^ stable_hash32(pass.depth_target) ^
      pass.clear_color ^ stable_hash32(resolve.source_target) ^
      stable_hash32(resolve.destination_target) ^ stable_hash32(resolve.mode) ^
      (render_target.sample_count * 0x01020408u) ^
      (destination_target.sample_count * 0x08040201u);
  context.material_salt = static_cast<std::uint32_t>(material.mate_id) ^
                          static_cast<std::uint32_t>(material.mate_id >> 32u);

  const bool follows_player_anchor = drawable.kind == "aircraft";
  const auto to_world = [&transform, &frame, follows_player_anchor](const DecodedVertex& vertex) noexcept {
    const float anchor_x = follows_player_anchor ? frame.position_x : 0.0f;
    const float anchor_y = follows_player_anchor ? frame.position_y : 0.0f;
    const float anchor_z = follows_player_anchor ? frame.position_z : 0.0f;
    return Vec3{vertex.x * transform.scale_x + transform.translate_x + anchor_x,
                vertex.y * transform.scale_y + transform.translate_y + anchor_y,
                vertex.z * transform.scale_z + transform.translate_z + anchor_z};
  };
  context.projected_vertices.resize(decoded.vertices.size());
  context.projected_valid.assign(decoded.vertices.size(), 0);
  for (std::uint32_t i = 0; i < decoded.vertices.size(); ++i) {
    const DecodedVertex& vertex = decoded.vertices[i];
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) {
      return false;
    }
    const Vec3 world = to_world(vertex);
    if (!std::isfinite(world.x) || !std::isfinite(world.y) || !std::isfinite(world.z)) {
      return false;
    }
    ScreenPoint screen{};
    const bool in_view = context.camera != nullptr
        ? project_clip_point(*context.camera, world, width_, height_, screen)
        : project_point(context.projection, world, width_, height_, screen);
    const bool valid = in_view || (context.camera != nullptr
        ? project_clip_point(*context.camera, world, width_, height_, screen, false)
        : project_point(context.projection, world, width_, height_, screen, false));
    if (valid) {
      context.projected_vertices[i] = screen;
      context.projected_valid[i] = 1;
      ++context.metrics->projected_vertices;
    }
  }
  return true;
}

std::uint32_t NativeRenderTarget::shade_fragment(const GeometryRasterContext& context,
                                                 std::uint32_t salt, float u, float v) const noexcept {
  const MissionMaterial& material = *context.material;
  const MissionTextureBinding& texture = *context.texture;
  std::uint32_t sampled = 0;
  const bool has_sample = context.texture_database != nullptr &&
      context.texture_database->sample(texture.mission_id, texture.stable_id, u, v, sampled);
  const std::uint32_t rgb = (has_sample ? sampled : material.base_color) & 0x00FFFFFFu;
  const std::uint32_t identity =
      salt ^ context.texture_salt ^ context.shader_salt ^ context.material_salt;
  const float gain = 0.78f + static_cast<float>(identity & 0x3Fu) / 256.0f;
  const auto modulate = [gain](std::uint32_t value) {
    return static_cast<std::uint32_t>(
        std::clamp(static_cast<float>(value) * gain, 0.0f, 255.0f));
  };
  return (material.base_color & 0xFF000000u) |
         (modulate((rgb >> 16u) & 0xFFu) << 16u) |
         (modulate((rgb >> 8u) & 0xFFu) << 8u) |
         modulate(rgb & 0xFFu);
}

bool NativeRenderTarget::write_projected_fragment(GeometryRasterContext& context,
                                                  std::uint32_t x, std::uint32_t y, float depth,
                                                  std::uint32_t salt, float u, float v) noexcept {
  NativeRasterDrawableMetrics& metrics = *context.metrics;
  const MissionMaterial& material = *context.material;
  if (x >= width_ || y >= height_ || !std::isfinite(depth)) return false;
  const std::size_t pixel = static_cast<std::size_t>(y) * width_ + x;
  const float depth_value = std::clamp(depth, 0.0f, 1.0f);
  if (material.depth_test && depth_value >= depth_[pixel]) return false;
  ++metrics.depth_pass_fragments;
  const std::uint32_t shaded = shade_fragment(context, salt, u, v);
  if (material.blend_mode == "opaque") {
    color_[pixel] = shaded;
  } else if (material.blend_mode == "alpha") {
    const std::uint32_t dst = color_[pixel];
    const std::uint32_t src_a = (shaded >> 24u) & 0xFFu;
    const std::uint32_t inv_a = 255u - src_a;
    const std::uint32_t src_r = (shaded >> 16u) & 0xFFu;
    const std::uint32_t src_g = (shaded >> 8u) & 0xFFu;
    const std::uint32_t src_b = shaded & 0xFFu;
    const std::uint32_t dst_r = (dst >> 16u) & 0xFFu;
    const std::uint32_t dst_g = (dst >> 8u) & 0xFFu;
    const std::uint32_t dst_b = dst & 0xFFu;
    color_[pixel] = 0xFF000000u |
                    (((src_r * src_a + dst_r * inv_a) / 255u) << 16u) |
                    (((src_g * src_a + dst_g * inv_a) / 255u) << 8u) |
                    ((src_b * src_a + dst_b * inv_a) / 255u);
  } else {
    const std::uint32_t dst = color_[pixel];
    const std::uint32_t r = std::min(255u, ((dst >> 16u) & 0xFFu) + ((shaded >> 16u) & 0xFFu));
    const std::uint32_t g = std::min(255u, ((dst >> 8u) & 0xFFu) + ((shaded >> 8u) & 0xFFu));
    const std::uint32_t b = std::min(255u, (dst & 0xFFu) + (shaded & 0xFFu));
    color_[pixel] = 0xFF000000u | (r << 16u) | (g << 8u) | b;
  }
  if (material.depth_write) depth_[pixel] = depth_value;
  object_ids_[pixel] = metrics.object_id;
  ++metrics.color_writes;
  ++filled_fragment_writes_;
  ++raster_writes_;
  if (pixel_stamps_[pixel] != raster_stamp_) {
    pixel_stamps_[pixel] = raster_stamp_;
    ++metrics.unique_pixels;
  }
  if (!metrics.screen_bbox_valid) {
    metrics.bbox_min_x = metrics.bbox_max_x = x;
    metrics.bbox_min_y = metrics.bbox_max_y = y;
    metrics.screen_bbox_valid = true;
  } else {
    metrics.bbox_min_x = std::min(metrics.bbox_min_x, x);
    metrics.bbox_min_y = std::min(metrics.bbox_min_y, y);
    metrics.bbox_max_x = std::max(metrics.bbox_max_x, x);
    metrics.bbox_max_y = std::max(metrics.bbox_max_y, y);
  }
  metrics.depth_min = std::min(metrics.depth_min, depth_value);
  metrics.depth_max = std::max(metrics.depth_max, depth_value);
  return true;
}

void NativeRenderTarget::rasterize_clipped_triangle(GeometryRasterContext& context,
                                                    const ClipVertex& clip_a,
                                                    const ClipVertex& clip_b,
                                                    const ClipVertex& clip_c,
                                                    std::uint32_t salt) {
  NativeRasterDrawableMetrics& metrics = *context.metrics;
  const auto screen_from_clip = [this](const ClipVertex& vertex) noexcept {
    const float safe_x = std::clamp(vertex.x, -1.0f, 1.0f);
    const float safe_y = std::clamp(vertex.y, -1.0f, 1.0f);
    return ScreenPoint{
        std::min(width_ - 1u, static_cast<std::uint32_t>(
                                  (safe_x * 0.5f + 0.5f) * static_cast<float>(width_))),
        std::min(height_ - 1u, static_cast<std::uint32_t>(
                                   (0.5f - safe_y * 0.5f) * static_cast<float>(height_))),
        std::clamp(vertex.z * 0.5f + 0.5f, 0.0f, 1.0f),
        vertex.x, vertex.y, vertex.z};
  };
  const ScreenPoint a = screen_from_clip(clip_a);
  const ScreenPoint b = screen_from_clip(clip_b);
  const ScreenPoint c = screen_from_clip(clip_c);
  const auto edge = [](float ax, float ay, float bx, float by, float px, float py) noexcept {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
  };
  const float area = edge(static_cast<float>(a.x), static_cast<float>(a.y),
                          static_cast<float>(b.x), static_cast<float>(b.y),
                          static_cast<float>(c.x), static_cast<float>(c.y));
  if (!std::isfinite(area) || std::abs(area) < 0.5f) {
    ++metrics.degenerate_triangles;
    return;
  }
  ++metrics.nondegenerate_triangles;
  ++raster_triangles_;
  const std::uint32_t min_x = std::min({a.x, b.x, c.x});
  const std::uint32_t max_x = std::max({a.x, b.x, c.x});
  const std::uint32_t min_y = std::min({a.y, b.y, c.y});
  const std::uint32_t max_y = std::max({a.y, b.y, c.y});
  const float inverse_area = 1.0f / area;
  for (std::uint32_t y = min_y; y <= max_y; ++y) {
    for (std::uint32_t x = min_x; x <= max_x; ++x) {
      const float px = static_cast<float>(x) + 0.5f;
      const float py = static_cast<float>(y) + 0.5f;
      ++metrics.fragment_tests;
      const float wa = edge(static_cast<float>(b.x), static_cast<float>(b.y),
                            static_cast<float>(c.x), static_cast<float>(c.y), px, py) * inverse_area;
      const float wb = edge(static_cast<float>(c.x), static_cast<float>(c.y),
                            static_cast<float>(a.x), static_cast<float>(a.y), px, py) * inverse_area;
      const float wc = edge(static_cast<float>(a.x), static_cast<float>(a.y),
                            static_cast<float>(b.x), static_cast<float>(b.y), px, py) * inverse_area;
      if (wa < -0.001f || wb < -0.001f || wc < -0.001f) continue;
      ++metrics.inside_fragments;
      const float depth = wa * a.depth + wb * b.depth + wc * c.depth;
      const float u = wa * clip_a.u + wb * clip_b.u + wc * clip_c.u;
      const float v = wa * clip_a.v + wb * clip_b.v + wc * clip_c.v;
      (void)write_projected_fragment(context, x, y, depth,
                                     context.drawable->asset ^ context.ordinal ^ salt, u, v);
    }
  }
}

bool NativeRenderTarget::rasterize_triangle(GeometryRasterContext& context,
                                             std::uint32_t ia, std::uint32_t ib,
                                             std::uint32_t ic, std::uint32_t salt) {
  NativeRasterDrawableMetrics& metrics = *context.metrics;
  const DecodedGeometry& decoded = *context.decoded;
  ++metrics.candidate_triangles;
  if (ia >= decoded.vertices.size() || ib >= decoded.vertices.size() ||
      ic >= decoded.vertices.size()) return false;
  if (!context.projected_valid[ia] || !context.projected_valid[ib] || !context.projected_valid[ic]) {
    ++metrics.clip_rejected_triangles;
    return true;
  }
  const ScreenPoint& a = context.projected_vertices[ia];
  const ScreenPoint& b = context.projected_vertices[ib];
  const ScreenPoint& c = context.projected_vertices[ic];
  if (!std::isfinite(a.ndc_x) || !std::isfinite(a.ndc_y) || !std::isfinite(a.ndc_z) ||
      !std::isfinite(b.ndc_x) || !std::isfinite(b.ndc_y) || !std::isfinite(b.ndc_z) ||
      !std::isfinite(c.ndc_x) || !std::isfinite(c.ndc_y) || !std::isfinite(c.ndc_z)) {
    ++metrics.clip_rejected_triangles;
    return true;
  }
  std::array<ClipVertex, 16> polygon{};
  polygon[0] = {a.ndc_x, a.ndc_y, a.ndc_z, decoded.vertices[ia].u, decoded.vertices[ia].v};
  polygon[1] = {b.ndc_x, b.ndc_y, b.ndc_z, decoded.vertices[ib].u, decoded.vertices[ib].v};
  polygon[2] = {c.ndc_x, c.ndc_y, c.ndc_z, decoded.vertices[ic].u, decoded.vertices[ic].v};
  std::size_t polygon_count = 3;
  const auto clip_coordinate = [](const ClipVertex& vertex, std::uint32_t axis) noexcept {
    return axis == 0 ? vertex.x : (axis == 1 ? vertex.y : vertex.z);
  };
  const auto interpolate_clip_vertex = [](const ClipVertex& left,
                                          const ClipVertex& right,
                                          float factor) noexcept {
    return ClipVertex{
        left.x + (right.x - left.x) * factor,
        left.y + (right.y - left.y) * factor,
        left.z + (right.z - left.z) * factor,
        left.u + (right.u - left.u) * factor,
        left.v + (right.v - left.v) * factor,
    };
  };
  const auto clip_plane = [&](std::uint32_t axis, float boundary, bool keep_greater) {
    std::array<ClipVertex, 16> input = polygon;
    std::size_t output_count = 0;
    const auto inside = [&](const ClipVertex& vertex) noexcept {
      const float value = clip_coordinate(vertex, axis);
      return keep_greater ? value >= boundary : value <= boundary;
    };
    const auto append = [&](const ClipVertex& vertex) {
      if (output_count < polygon.size()) polygon[output_count++] = vertex;
    };
    for (std::size_t index = 0; index < polygon_count; ++index) {
      const ClipVertex& current = input[index];
      const ClipVertex& next = input[(index + 1u) % polygon_count];
      const bool current_inside = inside(current);
      const bool next_inside = inside(next);
      if (current_inside != next_inside) {
        const float current_value = clip_coordinate(current, axis);
        const float next_value = clip_coordinate(next, axis);
        const float denominator = next_value - current_value;
        const float factor = std::abs(denominator) < 0.000001f
            ? 0.0f : (boundary - current_value) / denominator;
        append(interpolate_clip_vertex(current, next, std::clamp(factor, 0.0f, 1.0f)));
      }
      if (next_inside) append(next);
    }
    polygon_count = output_count;
  };
  clip_plane(0, -1.0f, true);
  clip_plane(0, 1.0f, false);
  clip_plane(1, -1.0f, true);
  clip_plane(1, 1.0f, false);
  clip_plane(2, -1.0f, true);
  clip_plane(2, 1.0f, false);
  if (polygon_count < 3) {
    ++metrics.clip_rejected_triangles;
    return true;
  }
  for (std::size_t index = 1; index + 1u < polygon_count; ++index) {
    rasterize_clipped_triangle(context, polygon[0], polygon[index], polygon[index + 1u],
                               salt ^ static_cast<std::uint32_t>(index));
  }
  return true;
}

bool NativeRenderTarget::rasterize_geometry_draw(GeometryRasterContext& context) {
  NativeRasterDrawableMetrics& metrics = *context.metrics;
  const DecodedGeometry& decoded = *context.decoded;
  if (context.geometry->topology == NativeIndexTopology::TriangleStripRestart) {
    std::array<std::uint32_t, 3> strip{};
    std::size_t strip_count = 0;
    for (std::size_t i = 0; i <= decoded.indices.size(); ++i) {
      const bool restart = i == decoded.indices.size() ||
          decoded.indices[i] == std::numeric_limits<std::uint32_t>::max();
      if (restart) {
        if (i < decoded.indices.size()) ++metrics.restart_markers;
        strip_count = 0;
        continue;
      }
      strip[strip_count++ % 3u] = decoded.indices[i];
      if (strip_count >= 3u) {
        std::uint32_t a = strip[(strip_count - 3u) % 3u];
        std::uint32_t b = strip[(strip_count - 2u) % 3u];
        const std::uint32_t c = strip[(strip_count - 1u) % 3u];
        if ((strip_count & 1u) == 0u) std::swap(a, b);
        if (!rasterize_triangle(context, a, b, c, static_cast<std::uint32_t>(i))) return false;
      }
    }
  } else {
    for (std::uint32_t i = 0; i + 2u < decoded.indices.size(); i += 3u) {
      const std::uint32_t ia = decoded.indices[i];
      const std::uint32_t ib = decoded.indices[i + 1u];
      const std::uint32_t ic = decoded.indices[i + 2u];
      if (ia == std::numeric_limits<std::uint32_t>::max() ||
          ib == std::numeric_limits<std::uint32_t>::max() ||
          ic == std::numeric_limits<std::uint32_t>::max()) continue;
      if (!rasterize_triangle(context, ia, ib, ic, i)) return false;
    }
  }
  return true;
}

bool NativeRenderTarget::draw_world_geometry(const WorldFrame& frame,
                                             const MissionDrawable& drawable,
                                             const NativeGeometryMetadata& geometry,
                                             const DecodedGeometry& decoded,
                                             const MissionDrawableTransform& transform,
                                             const MissionMaterial& material,
                                             const MissionTextureBinding& texture,
                                             const ShaderPermutation& shader,
                                             const MissionRenderTargetDefinition& render_target,
                                             const MissionRenderTargetDefinition& destination_target,
                                             const MissionRenderPass& pass,
                                             const MissionRenderResolve& resolve,
                                             const MissionCameraDefinition* camera,
                                             const MissionTextureDatabase* texture_database,
                                             std::uint32_t ordinal) {
  GeometryRasterContext context;
  context.frame = &frame;
  context.drawable = &drawable;
  context.geometry = &geometry;
  context.decoded = &decoded;
  context.transform = &transform;
  context.material = &material;
  context.texture = &texture;
  context.shader = &shader;
  context.render_target = &render_target;
  context.destination_target = &destination_target;
  context.pass = &pass;
  context.resolve = &resolve;
  context.camera = camera;
  context.texture_database = texture_database;
  context.ordinal = ordinal;
  return prepare_geometry_draw(context) && rasterize_geometry_draw(context);
}
RenderReadback NativeRenderTarget::readback() const noexcept {
  RenderReadback readback{width_, height_, 0, 0, 1469598103934665603ull, 1469598103934665603ull};
  if (width_ == 0 || height_ == 0 || color_.size() != depth_.size()) return readback;
  for (std::size_t i = 0; i < color_.size(); ++i) {
    const std::uint32_t color = color_[i];
    readback.color_hash ^= color;
    readback.color_hash *= 1099511628211ull;
    if (color != 0) ++readback.color_coverage;

    const auto depth_quantized = static_cast<std::uint32_t>(
        std::max(0.0f, std::min(depth_[i], 1.0f)) * 16777215.0f);
    readback.depth_hash ^= depth_quantized;
    readback.depth_hash *= 1099511628211ull;
    if (depth_[i] < 1.0f) ++readback.depth_coverage;
  }
  return readback;
}

bool NativeRenderTarget::copy_rgba8(std::vector<std::uint8_t>& pixels) const {
  if (width_ == 0 || height_ == 0 || color_.size() != static_cast<std::size_t>(width_) * height_) {
    return false;
  }
  std::vector<std::uint8_t> converted;
  converted.resize(color_.size() * 4u);
  for (std::size_t i = 0; i < color_.size(); ++i) {
    const std::uint32_t pixel = color_[i];
    converted[i * 4u + 0u] = static_cast<std::uint8_t>((pixel >> 16u) & 0xFFu);
    converted[i * 4u + 1u] = static_cast<std::uint8_t>((pixel >> 8u) & 0xFFu);
    converted[i * 4u + 2u] = static_cast<std::uint8_t>(pixel & 0xFFu);
    converted[i * 4u + 3u] = static_cast<std::uint8_t>((pixel >> 24u) & 0xFFu);
  }
  pixels = std::move(converted);
  return true;
}

bool NativeRenderTarget::blit_argb32(
    std::uint32_t source_width, std::uint32_t source_height,
    std::span<const std::uint32_t> pixels, std::span<const float> depth,
    float far_plane) noexcept {
  if (source_width == 0 || source_height == 0 || width_ != source_width ||
      height_ != source_height || pixels.size() !=
          static_cast<std::size_t>(source_width) * source_height ||
      depth.size() != pixels.size() || !std::isfinite(far_plane) ||
      far_plane <= 0.0F || color_.size() != pixels.size() ||
      depth_.size() != pixels.size()) {
    return false;
  }
  std::copy(pixels.begin(), pixels.end(), color_.begin());
  for (std::size_t index = 0; index < depth.size(); ++index) {
    const float value = depth[index];
    depth_[index] = std::isfinite(value)
                        ? std::clamp(value / far_plane, 0.0F, 1.0F)
                        : 1.0F;
  }
  std::fill(object_ids_.begin(), object_ids_.end(), 0);
  std::fill(pixel_stamps_.begin(), pixel_stamps_.end(), 0);
  std::fill(hud_pixel_stamps_.begin(), hud_pixel_stamps_.end(), 0);
  geometry_calls_ = 0;
  raster_triangles_ = 0;
  raster_writes_ = 0;
  diagnostic_point_writes_ = 0;
  filled_fragment_writes_ = 0;
  raster_stamp_ = 0;
  hud_stamp_ = 0;
  hud_pixel_writes_ = 0;
  hud_unique_pixels_ = 0;
  raster_metrics_.clear();
  return true;
}

bool NativeRenderTarget::copy_depth(std::vector<float>& depth) const {
  if (width_ == 0 || height_ == 0 ||
      depth_.size() != static_cast<std::size_t>(width_) * height_) return false;
  depth = depth_;
  return true;
}

bool NativeRenderTarget::copy_object_id(std::vector<std::uint32_t>& object_ids) const {
  if (width_ == 0 || height_ == 0 ||
      object_ids_.size() != static_cast<std::size_t>(width_) * height_) return false;
  object_ids = object_ids_;
  return true;
}

bool NativeRenderTarget::write_ppm(const std::filesystem::path& path) const noexcept {
  if (path.empty() || width_ == 0 || height_ == 0 ||
      color_.size() != static_cast<std::size_t>(width_) * height_) {
    return false;
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output << "P6\n" << width_ << ' ' << height_ << "\n255\n";
  if (!output) return false;
  for (const std::uint32_t pixel : color_) {
    const unsigned char rgb[3] = {
        static_cast<unsigned char>((pixel >> 16u) & 0xFFu),
        static_cast<unsigned char>((pixel >> 8u) & 0xFFu),
        static_cast<unsigned char>(pixel & 0xFFu)};
    output.write(reinterpret_cast<const char*>(rgb), sizeof(rgb));
    if (!output) return false;
  }
  return true;
}

bool NativeRenderTarget::write_object_id_ppm(const std::filesystem::path& path) const noexcept {
  if (path.empty() || width_ == 0 || height_ == 0 ||
      object_ids_.size() != static_cast<std::size_t>(width_) * height_) {
    return false;
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output << "P6\n" << width_ << ' ' << height_ << "\n255\n";
  if (!output) return false;
  for (const std::uint32_t object_id : object_ids_) {
    const unsigned char rgb[3] = {
        static_cast<unsigned char>((object_id >> 16u) & 0xFFu),
        static_cast<unsigned char>((object_id >> 8u) & 0xFFu),
        static_cast<unsigned char>(object_id & 0xFFu)};
    output.write(reinterpret_cast<const char*>(rgb), sizeof(rgb));
    if (!output) return false;
  }
  return true;
}

bool NativeRenderTarget::write_depth_f32(const std::filesystem::path& path) const noexcept {
  if (path.empty() || width_ == 0 || height_ == 0 ||
      depth_.size() != static_cast<std::size_t>(width_) * height_) {
    return false;
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  output.write(reinterpret_cast<const char*>(depth_.data()),
               static_cast<std::streamsize>(depth_.size() * sizeof(float)));
  return output.good();
}


}  // namespace ac6
