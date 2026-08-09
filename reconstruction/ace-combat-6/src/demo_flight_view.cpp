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
                      const DemoCamera& camera,
                      const ac6::retail::FlightPosition& position) noexcept {
  image.clear(24, 32, 56);                       // sky, invented

  // The eye, from the contracted integrator. at68 is the vertical one.
  const Vec3 eye{position.at64, position.at68, position.at72};
  const float spacing = camera.invented_grid_spacing;
  const float half = spacing *
                     static_cast<float>(camera.invented_grid_lines - 1) * 0.5F;

  // THE GRID FOLLOWS THE EYE, snapped to the spacing so the lines do not crawl.
  // A finite grid under a moving aircraft is empty in ten seconds; this is a
  // rendering choice and it moves nothing but the lines.
  const float centre_x = std::floor(eye.x / spacing) * spacing;
  const float centre_z = std::floor(eye.z / spacing) * spacing;

  // Every point is drawn RELATIVE TO THE EYE, which is what makes the aircraft
  // move through the world rather than the world rotate around it.
  const auto relative = [&eye](float x, float y, float z) {
    return Vec3{x - eye.x, y - eye.y, z - eye.z};
  };

  for (int i = 0; i < camera.invented_grid_lines; ++i) {
    const float tx = centre_x - half + spacing * static_cast<float>(i);
    const float tz = centre_z - half + spacing * static_cast<float>(i);
    for (int s = 0; s + 1 < camera.invented_grid_lines; ++s) {
      const float ax = centre_x - half + spacing * static_cast<float>(s);
      const float az = centre_z - half + spacing * static_cast<float>(s);
      draw_segment(image, basis, camera, relative(tx, 0.0F, az),
                   relative(tx, 0.0F, az + spacing), 90, 130, 90);
      draw_segment(image, basis, camera, relative(ax, 0.0F, tz),
                   relative(ax + spacing, 0.0F, tz), 70, 105, 70);
    }
  }

  // A horizon ring at eye level, far out, so that roll and pitch are readable
  // even when the grid is out of frame. It is drawn relative to the eye and so
  // never runs out.
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

void draw_flight_view(Image& image, const ac6::retail::RetailBasis& basis,
                      const DemoCamera& camera) noexcept {
  // The stationary overload: an eye `invented_altitude` over the origin, which
  // is exactly what this function did before the position existed.
  ac6::retail::FlightPosition fixed{};
  fixed.at68 = camera.invented_altitude;
  draw_flight_view(image, basis, camera, fixed);
}

void draw_mesh_wireframe(Image& image, const ac6::retail::NdxrMesh& mesh,
                         const ac6::retail::RetailBasis& basis,
                         const DemoCamera& camera, float distance) noexcept {
  image.clear(16, 18, 26);
  if (mesh.positions.empty() || mesh.indices.size() < 2 || !mesh.bounds.valid) return;

  // Framing is CHOSEN: centre the model on its own bounds and pull back by a
  // multiple of its radius, so a 2-unit part and a 200-unit one both fill the
  // frame. Nothing about the model's real scale is claimed by this.
  const float cx = (mesh.bounds.min_x + mesh.bounds.max_x) * 0.5F;
  const float cy = (mesh.bounds.min_y + mesh.bounds.max_y) * 0.5F;
  const float cz = (mesh.bounds.min_z + mesh.bounds.max_z) * 0.5F;

  // THE MODEL TURNS, NOT THE CAMERA. `basis` is applied to the point AFTER it
  // is centred and BEFORE it is pushed away, so the mesh spins in place at a
  // fixed distance. Handing the basis to `draw_segment` instead swings the model
  // out of frame -- the first turntable drew 2738 pixels at frame 0 and zero at
  // frames 30 and 60 for exactly that reason.
  //
  // +distance, not -: `project` refuses anything with z <= 1, so the forward
  // axis is positive and the model has to be pushed away along it. The run
  // before that one drew zero pixels everywhere.
  const auto at = [&](std::uint16_t index) {
    const ac6::retail::NdxrPosition& p = mesh.positions[index];
    const Vec3 turned = to_camera(basis, Vec3{p.x - cx, p.y - cy, p.z - cz});
    return Vec3{turned.x, turned.y, turned.z + distance};
  };
  const ac6::retail::RetailBasis fixed = ac6::retail::identity_basis();

  // THE STRIP, walked with retail's restart. Every consecutive pair inside a
  // run is an edge; 0xFFFF ends the run and the next two values start a new one.
  std::uint16_t previous = ac6::retail::kStripRestart;
  for (const std::uint16_t index : mesh.indices) {
    if (index == ac6::retail::kStripRestart) { previous = index; continue; }
    if (previous != ac6::retail::kStripRestart) {
      draw_segment(image, fixed, camera, at(previous), at(index), 150, 200, 235);
    }
    previous = index;
  }
}

void draw_mesh_at(Image& image, const ac6::retail::NdxrMesh& mesh,
                  const ac6::retail::RetailBasis& basis, const DemoCamera& camera,
                  float ox, float oy, float oz,
                  std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
  if (mesh.positions.empty() || mesh.indices.size() < 2) return;
  const auto at = [&](std::uint16_t index) {
    const ac6::retail::NdxrPosition& p = mesh.positions[index];
    return Vec3{p.x + ox, p.y + oy, p.z + oz};
  };
  std::uint16_t previous = ac6::retail::kStripRestart;
  for (const std::uint16_t index : mesh.indices) {
    if (index == ac6::retail::kStripRestart) { previous = index; continue; }
    if (previous != ac6::retail::kStripRestart) {
      draw_segment(image, basis, camera, at(previous), at(index), r, g, b);
    }
    previous = index;
  }
}

std::string caption() noexcept {
  // Updated at cycle 1416. The aircraft MOVES now: cycle 1415 established that
  // the integrator's rates are a unit direction and its scale is a speed, and
  // wired the contracted integrator into the session. So the clause saying the
  // position step is blocked is gone -- it named 0x823042D0, the live model's
  // own step, which is a different function. What is blocked is the DIRECTION
  // retail feeds the integrator, and that is what the caption now says.
  return "Attitude and control response: Ace Combat 6's own, from the "
         "controller record to the aeroplane's orientation, ported function by "
         "function and verified bit-for-bit against the retail instructions by "
         "micro-execution -- 29 contracted behaviours. "
         "Invented for this picture: the camera, the scene, which basis row is "
         "which axis, which controller axis feeds which binding slot, and the "
         "heading and speed the aircraft is flown at. "
         "Retail's gameplay camera uses estimate instructions whose exact "
         "results belong to the console, so it is not reproduced. "
         "The aircraft moves under retail's own integrator -- its 1/3.6 scale, "
         "its 10.0 floor, its fused steps -- but the direction that integrator "
         "is fed comes from a vector normalise seeded on the same estimates, "
         "so the heading here is chosen and the flying of it is not.";
}

}  // namespace ac6::demo
