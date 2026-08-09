// A picture of the contracted flight model. Nothing here is ported.

#include "ac6/demo_flight_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ac6::demo {
namespace {

struct Vec3 {
  float x{}, y{}, z{};
};

Vec3 to_camera(const ac6::retail::RetailBasis& basis, const Vec3& world) noexcept {
  // MY CHOICE, and the header says so: row 0 is right, row 1 is up, row 2 is
  // forward. Nothing in the campaign establishes this.
  const auto& r = basis.rows[0];
  const auto& u = basis.rows[1];
  const auto& f = basis.rows[2];
  return Vec3{world.x * r[0] + world.y * r[1] + world.z * r[2],
              world.x * u[0] + world.y * u[1] + world.z * u[2],
              world.x * f[0] + world.y * f[1] + world.z * f[2]};
}

bool project(const Vec3& camera_space, const DemoCamera& camera, int width,
             int height, int& sx, int& sy) noexcept {
  if (camera_space.z <= 1.0F) {
    return false;                      // behind, or too close
  }
  const float scale = (static_cast<float>(height) * 0.5F) /
                      std::tan(camera.invented_fov_y * 0.5F);
  sx = static_cast<int>(static_cast<float>(width) * 0.5F +
                        camera_space.x * scale / camera_space.z);
  sy = static_cast<int>(static_cast<float>(height) * 0.5F -
                        camera_space.y * scale / camera_space.z);
  return true;
}

void draw_segment(Image& image, const ac6::retail::RetailBasis& basis,
                  const DemoCamera& camera, const Vec3& a, const Vec3& b,
                  std::uint8_t r, std::uint8_t g, std::uint8_t bl) noexcept {
  int ax = 0, ay = 0, bx = 0, by = 0;
  const Vec3 ca = to_camera(basis, a);
  const Vec3 cb = to_camera(basis, b);
  if (!project(ca, camera, image.width, image.height, ax, ay)) { return; }
  if (!project(cb, camera, image.width, image.height, bx, by)) { return; }
  image.line(ax, ay, bx, by, r, g, bl);
}

}  // namespace

void Image::clear(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
  rgb.assign(static_cast<std::size_t>(width) * height * 3, 0);
  for (std::size_t i = 0; i + 2 < rgb.size(); i += 3) {
    rgb[i] = r; rgb[i + 1] = g; rgb[i + 2] = b;
  }
}

void Image::plot(int x, int y, std::uint8_t r, std::uint8_t g,
                 std::uint8_t b) noexcept {
  if (x < 0 || y < 0 || x >= width || y >= height) { return; }
  const std::size_t i = (static_cast<std::size_t>(y) * width + x) * 3;
  rgb[i] = r; rgb[i + 1] = g; rgb[i + 2] = b;
}

void Image::line(int x0, int y0, int x1, int y1, std::uint8_t r,
                 std::uint8_t g, std::uint8_t b) noexcept {
  // Bresenham, and bounded: a line whose endpoints are far off-screen must not
  // spin for a million iterations.
  const int dx = std::abs(x1 - x0);
  const int dy = -std::abs(y1 - y0);
  const int sx = x0 < x1 ? 1 : -1;
  const int sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  int guard = 4 * (width + height);
  while (guard-- > 0) {
    plot(x0, y0, r, g, b);
    if (x0 == x1 && y0 == y1) { break; }
    const int doubled = 2 * error;
    if (doubled >= dy) { error += dy; x0 += sx; }
    if (doubled <= dx) { error += dx; y0 += sy; }
  }
}

bool Image::write_ppm(const char* path) const {
  std::FILE* out = std::fopen(path, "wb");
  if (out == nullptr) { return false; }
  std::fprintf(out, "P6\n%d %d\n255\n", width, height);
  std::fwrite(rgb.data(), 1, rgb.size(), out);
  std::fclose(out);
  return true;
}

void draw_flight_view(Image& image, const ac6::retail::RetailBasis& basis,
                      const DemoCamera& camera) noexcept {
  image.clear(24, 32, 56);                       // sky, invented

  const float half = camera.invented_grid_spacing *
                     static_cast<float>(camera.invented_grid_lines - 1) * 0.5F;
  const float ground = -camera.invented_altitude;

  for (int i = 0; i < camera.invented_grid_lines; ++i) {
    const float t = -half + camera.invented_grid_spacing * static_cast<float>(i);
    // Lines along the forward axis, and lines across it. Both are drawn in
    // segments so that a line crossing the near plane is clipped by the
    // per-segment test rather than by projecting an endpoint behind the eye.
    for (int s = 0; s + 1 < camera.invented_grid_lines; ++s) {
      const float u0 = -half + camera.invented_grid_spacing * static_cast<float>(s);
      const float u1 = u0 + camera.invented_grid_spacing;
      draw_segment(image, basis, camera, Vec3{t, ground, u0},
                   Vec3{t, ground, u1}, 90, 130, 90);
      draw_segment(image, basis, camera, Vec3{u0, ground, t},
                   Vec3{u1, ground, t}, 70, 105, 70);
    }
  }

  // A horizon ring, far out, so that roll and pitch are readable even when the
  // grid is out of frame.
  const float radius = 8000.0F;
  int previous_x = 0, previous_y = 0;
  bool have_previous = false;
  for (int step = 0; step <= 128; ++step) {
    const float angle = 6.2831853F * static_cast<float>(step) / 128.0F;
    const Vec3 point{radius * std::cos(angle), 0.0F, radius * std::sin(angle)};
    int x = 0, y = 0;
    const bool visible = project(to_camera(basis, point), camera, image.width,
                                 image.height, x, y);
    if (visible && have_previous) {
      image.line(previous_x, previous_y, x, y, 220, 200, 140);
    }
    previous_x = x; previous_y = y; have_previous = visible;
  }
}

std::string caption() noexcept {
  return "Attitude: Ace Combat 6's own flight model, ported function by "
         "function and verified bit-for-bit against the retail instructions by "
         "micro-execution (25 contracted behaviours). "
         "Camera, scene, axis assignment and field of view: invented for this "
         "picture. Retail's gameplay camera uses estimate instructions whose "
         "exact results belong to the console, so it is not reproduced. "
         "The aircraft changes attitude and does not move: its position step "
         "depends on the same estimates.";
}

}  // namespace ac6::demo
