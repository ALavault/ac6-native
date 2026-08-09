// A picture of the contracted flight model. Nothing here is ported.

#include "ac6/demo_flight_view.h"

#include <algorithm>
#include <cmath>
#include <limits>
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

void Image::clear_depth() noexcept {
  depth.assign(static_cast<std::size_t>(width) * height,
               std::numeric_limits<float>::infinity());
}

void Image::triangle(int x0, int y0, float z0, int x1, int y1, float z1,
                     int x2, int y2, float z2,
                     std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
  if (depth.size() != static_cast<std::size_t>(width) * height) return;
  const int min_x = std::max(0, std::min(x0, std::min(x1, x2)));
  const int max_x = std::min(width - 1, std::max(x0, std::max(x1, x2)));
  const int min_y = std::max(0, std::min(y0, std::min(y1, y2)));
  const int max_y = std::min(height - 1, std::max(y0, std::max(y1, y2)));
  if (min_x > max_x || min_y > max_y) return;
  const float area = static_cast<float>((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0));
  if (area == 0.0F) return;                     // degenerate, and strips make many
  for (int y = min_y; y <= max_y; ++y) {
    for (int x = min_x; x <= max_x; ++x) {
      const float w0 = static_cast<float>((x1 - x) * (y2 - y) - (x2 - x) * (y1 - y)) / area;
      const float w1 = static_cast<float>((x2 - x) * (y0 - y) - (x0 - x) * (y2 - y)) / area;
      const float w2 = 1.0F - w0 - w1;
      // Both signs accepted: no winding rule is read, so a face is not
      // discarded for facing away.
      if ((w0 < 0.0F || w1 < 0.0F || w2 < 0.0F) &&
          (w0 > 0.0F || w1 > 0.0F || w2 > 0.0F)) continue;
      const float z = w0 * z0 + w1 * z1 + w2 * z2;
      const std::size_t at = static_cast<std::size_t>(y) * width + x;
      if (!(z < depth[at])) continue;
      depth[at] = z;
      rgb[at * 3] = r; rgb[at * 3 + 1] = g; rgb[at * 3 + 2] = b;
    }
  }
}

void Image::triangle_textured(int x0, int y0, float z0, float u0, float v0,
                              int x1, int y1, float z1, float u1, float v1,
                              int x2, int y2, float z2, float u2, float v2,
                              const std::uint32_t* texels, int tw, int th,
                              float shade) noexcept {
  if (depth.size() != static_cast<std::size_t>(width) * height) return;
  if (texels == nullptr || tw <= 0 || th <= 0) return;
  const int min_x = std::max(0, std::min(x0, std::min(x1, x2)));
  const int max_x = std::min(width - 1, std::max(x0, std::max(x1, x2)));
  const int min_y = std::max(0, std::min(y0, std::min(y1, y2)));
  const int max_y = std::min(height - 1, std::max(y0, std::max(y1, y2)));
  if (min_x > max_x || min_y > max_y) return;
  const float area = static_cast<float>((x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0));
  if (area == 0.0F) return;
  for (int y = min_y; y <= max_y; ++y) {
    for (int x = min_x; x <= max_x; ++x) {
      const float w0 = static_cast<float>((x1 - x) * (y2 - y) - (x2 - x) * (y1 - y)) / area;
      const float w1 = static_cast<float>((x2 - x) * (y0 - y) - (x0 - x) * (y2 - y)) / area;
      const float w2 = 1.0F - w0 - w1;
      if ((w0 < 0.0F || w1 < 0.0F || w2 < 0.0F) &&
          (w0 > 0.0F || w1 > 0.0F || w2 > 0.0F)) continue;
      const float z = w0 * z0 + w1 * z1 + w2 * z2;
      const std::size_t at = static_cast<std::size_t>(y) * width + x;
      if (!(z < depth[at])) continue;
      // repeat wrapping, chosen -- see the header.
      float u = w0 * u0 + w1 * u1 + w2 * u2;
      float v = w0 * v0 + w1 * v1 + w2 * v2;
      u -= std::floor(u);
      v -= std::floor(v);
      int tx = static_cast<int>(u * static_cast<float>(tw));
      int ty = static_cast<int>(v * static_cast<float>(th));
      if (tx < 0) tx = 0; if (tx >= tw) tx = tw - 1;
      if (ty < 0) ty = 0; if (ty >= th) ty = th - 1;
      const std::uint32_t texel = texels[static_cast<std::size_t>(ty) * tw + tx];
      // ALPHA TEST. Cycle 1478 measured the map package: 28 of 170 models carry
      // a texture with more than 2% of its texels below alpha 128, and one is
      // 93.4% transparent. Drawing those opaque turns a lattice -- a bridge's
      // hangers, a fence, a tree -- into a solid slab, which is exactly what the
      // grey faces in cycles 1475-1477 were. The cutoff is mine; the alpha is
      // retail's, and 0xAABBGGRR is the decoder's own byte order.
      if ((texel >> 24) < 128) continue;
      const auto ch = [&](std::uint32_t v8) {
        const float f = static_cast<float>(v8) * shade;
        return static_cast<std::uint8_t>(f < 0.0F ? 0.0F : (f > 255.0F ? 255.0F : f));
      };
      depth[at] = z;
      rgb[at * 3] = ch(texel & 0xFF);              // 0xAABBGGRR -> R is low
      rgb[at * 3 + 1] = ch((texel >> 8) & 0xFF);
      rgb[at * 3 + 2] = ch((texel >> 16) & 0xFF);
    }
  }
}

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

void draw_mesh_solid(Image& image, const ac6::retail::NdxrMesh& mesh,
                     const ac6::retail::RetailBasis& basis, const DemoCamera& camera,
                     float distance) noexcept {
  image.clear(14, 16, 24);
  image.clear_depth();
  if (mesh.positions.empty() || mesh.indices.size() < 3 || !mesh.bounds.valid) return;
  const bool lit = mesh.normals.size() == mesh.positions.size();

  const float cx = (mesh.bounds.min_x + mesh.bounds.max_x) * 0.5F;
  const float cy = (mesh.bounds.min_y + mesh.bounds.max_y) * 0.5F;
  const float cz = (mesh.bounds.min_z + mesh.bounds.max_z) * 0.5F;
  const auto at = [&](std::uint16_t index) {
    const ac6::retail::NdxrPosition& p = mesh.positions[index];
    const Vec3 turned = to_camera(basis, Vec3{p.x - cx, p.y - cy, p.z - cz});
    return Vec3{turned.x, turned.y, turned.z + distance};
  };
  const float lx = 0.4F, ly = 0.7F, lz = -0.6F;
  const auto shade = [&](std::uint16_t index) {
    if (!lit) return 0.7F;
    const ac6::retail::NdxrPosition& n = mesh.normals[index];
    const Vec3 t = to_camera(basis, Vec3{n.x, n.y, n.z});
    const float d = t.x * lx + t.y * ly + t.z * lz;
    return 0.25F + 0.75F * std::fabs(d);        // two-sided: no winding is read
  };

  std::uint16_t a = ac6::retail::kStripRestart;
  std::uint16_t bb = ac6::retail::kStripRestart;
  int parity = 0;
  for (const std::uint16_t index : mesh.indices) {
    if (index == ac6::retail::kStripRestart) {
      a = bb = ac6::retail::kStripRestart; parity = 0; continue;
    }
    if (a != ac6::retail::kStripRestart && bb != ac6::retail::kStripRestart) {
      // The strip's alternating winding. It changes nothing here because
      // neither side is culled, and it is kept because the format has it.
      const std::uint16_t i0 = a;
      const std::uint16_t i1 = (parity == 0) ? bb : index;
      const std::uint16_t i2 = (parity == 0) ? index : bb;
      int ax = 0, ay = 0, bx = 0, by = 0, cx2 = 0, cy2 = 0;
      const Vec3 pa = at(i0), pb = at(i1), pc = at(i2);
      if (project(pa, camera, image.width, image.height, ax, ay) &&
          project(pb, camera, image.width, image.height, bx, by) &&
          project(pc, camera, image.width, image.height, cx2, cy2)) {
        const float k = (shade(i0) + shade(i1) + shade(i2)) / 3.0F;
        const auto ch = [&](float base) {
          const float v = base * k;
          return static_cast<std::uint8_t>(v < 0.0F ? 0.0F : (v > 255.0F ? 255.0F : v));
        };
        image.triangle(ax, ay, pa.z, bx, by, pb.z, cx2, cy2, pc.z,
                       ch(205.0F), ch(220.0F), ch(245.0F));
      }
      parity ^= 1;
    }
    a = bb; bb = index;
  }
}

void draw_mesh_textured(Image& image, const ac6::retail::NdxrMesh& mesh,
                        const ac6::retail::RetailBasis& basis,
                        const DemoCamera& camera, float distance,
                        const std::uint32_t* texels, int tw, int th) noexcept {
  image.clear(14, 16, 24);
  image.clear_depth();
  if (mesh.positions.empty() || mesh.indices.size() < 3 || !mesh.bounds.valid) return;
  if (mesh.texcoords.size() != mesh.positions.size() || texels == nullptr) {
    draw_mesh_solid(image, mesh, basis, camera, distance);
    return;
  }
  const bool lit = mesh.normals.size() == mesh.positions.size();
  const float cx = (mesh.bounds.min_x + mesh.bounds.max_x) * 0.5F;
  const float cy = (mesh.bounds.min_y + mesh.bounds.max_y) * 0.5F;
  const float cz = (mesh.bounds.min_z + mesh.bounds.max_z) * 0.5F;
  const auto at = [&](std::uint16_t index) {
    const ac6::retail::NdxrPosition& p = mesh.positions[index];
    const Vec3 turned = to_camera(basis, Vec3{p.x - cx, p.y - cy, p.z - cz});
    return Vec3{turned.x, turned.y, turned.z + distance};
  };
  const float lx = 0.4F, ly = 0.7F, lz = -0.6F;
  const auto shade = [&](std::uint16_t index) {
    if (!lit) return 0.85F;
    const ac6::retail::NdxrPosition& n = mesh.normals[index];
    const Vec3 t = to_camera(basis, Vec3{n.x, n.y, n.z});
    return 0.45F + 0.55F * std::fabs(t.x * lx + t.y * ly + t.z * lz);
  };
  std::uint16_t a = ac6::retail::kStripRestart;
  std::uint16_t bb = ac6::retail::kStripRestart;
  int parity = 0;
  for (const std::uint16_t index : mesh.indices) {
    if (index == ac6::retail::kStripRestart) {
      a = bb = ac6::retail::kStripRestart; parity = 0; continue;
    }
    if (a != ac6::retail::kStripRestart && bb != ac6::retail::kStripRestart) {
      const std::uint16_t i0 = a;
      const std::uint16_t i1 = (parity == 0) ? bb : index;
      const std::uint16_t i2 = (parity == 0) ? index : bb;
      int ax = 0, ay = 0, bx = 0, by = 0, cx2 = 0, cy2 = 0;
      const Vec3 pa = at(i0), pb = at(i1), pc = at(i2);
      if (project(pa, camera, image.width, image.height, ax, ay) &&
          project(pb, camera, image.width, image.height, bx, by) &&
          project(pc, camera, image.width, image.height, cx2, cy2)) {
        const float k = (shade(i0) + shade(i1) + shade(i2)) / 3.0F;
        image.triangle_textured(
            ax, ay, pa.z, mesh.texcoords[i0].u, mesh.texcoords[i0].v,
            bx, by, pb.z, mesh.texcoords[i1].u, mesh.texcoords[i1].v,
            cx2, cy2, pc.z, mesh.texcoords[i2].u, mesh.texcoords[i2].v,
            texels, tw, th, k);
      }
      parity ^= 1;
    }
    a = bb; bb = index;
  }
}

void draw_mesh_lit(Image& image, const ac6::retail::NdxrMesh& mesh,
                   const ac6::retail::RetailBasis& basis, const DemoCamera& camera,
                   float distance) noexcept {
  image.clear(14, 16, 24);
  if (mesh.positions.empty() || mesh.indices.size() < 2 || !mesh.bounds.valid) return;
  if (mesh.normals.size() != mesh.positions.size()) {
    draw_mesh_wireframe(image, mesh, basis, camera, distance);
    return;
  }
  const float cx = (mesh.bounds.min_x + mesh.bounds.max_x) * 0.5F;
  const float cy = (mesh.bounds.min_y + mesh.bounds.max_y) * 0.5F;
  const float cz = (mesh.bounds.min_z + mesh.bounds.max_z) * 0.5F;
  const auto at = [&](std::uint16_t index) {
    const ac6::retail::NdxrPosition& p = mesh.positions[index];
    const Vec3 turned = to_camera(basis, Vec3{p.x - cx, p.y - cy, p.z - cz});
    return Vec3{turned.x, turned.y, turned.z + distance};
  };
  // CHOSEN: a light over the viewer's shoulder, and a ramp that keeps the
  // unlit side visible rather than black.
  const float lx = 0.4F, ly = 0.7F, lz = -0.6F;
  const auto shade = [&](std::uint16_t index) {
    const ac6::retail::NdxrPosition& n = mesh.normals[index];
    const Vec3 t = to_camera(basis, Vec3{n.x, n.y, n.z});
    const float d = t.x * lx + t.y * ly + t.z * lz;
    return 0.35F + 0.65F * (d * 0.5F + 0.5F);
  };
  const ac6::retail::RetailBasis fixed = ac6::retail::identity_basis();
  std::uint16_t previous = ac6::retail::kStripRestart;
  for (const std::uint16_t index : mesh.indices) {
    if (index == ac6::retail::kStripRestart) { previous = index; continue; }
    if (previous != ac6::retail::kStripRestart) {
      const float k = (shade(previous) + shade(index)) * 0.5F;
      const auto channel = [&](float base) {
        const float v = base * k;
        return static_cast<std::uint8_t>(v < 0.0F ? 0.0F : (v > 255.0F ? 255.0F : v));
      };
      draw_segment(image, fixed, camera, at(previous), at(index),
                   channel(210.0F), channel(226.0F), channel(255.0F));
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

namespace ac6::demo {

void draw_terrain_view(Image& image, const ac6::retail::RetailBasis& basis,
                       const DemoCamera& camera,
                       const ac6::retail::FlightPosition& position,
                       const ac6::retail::TerrainField& field,
                       const ac6::retail::MapWaterGrid* water) noexcept {
  using ac6::retail::kTerrainSampleUnits;
  using ac6::retail::kTerrainWorldBias;
  using ac6::retail::sample_is_present;

  image.clear(150, 172, 198);                      // sky, invented
  image.clear_depth();
  draw_terrain_view_over(image, basis, camera, position, field, water);
}

void draw_terrain_view_over(Image& image, const ac6::retail::RetailBasis& basis,
                            const DemoCamera& camera,
                            const ac6::retail::FlightPosition& position,
                            const ac6::retail::TerrainField& field,
                            const ac6::retail::MapWaterGrid* water) noexcept {
  using ac6::retail::kTerrainSampleUnits;
  using ac6::retail::kTerrainWorldBias;
  using ac6::retail::sample_is_present;

  // at64 -> x and at72 -> z. Which is north is unestablished; see the header.
  const float eye_x = position.at64;
  const float eye_y = position.at68;
  const float eye_z = position.at72;

  const float step = kTerrainSampleUnits;
  const long side = static_cast<long>(ac6::retail::TerrainField::field_side()) - 1;
  const long cx = static_cast<long>((eye_x + kTerrainWorldBias) / step);
  const long cz = static_cast<long>((eye_z + kTerrainWorldBias) / step);
  const long reach = 96;                           // samples, invented

  auto place = [&](float wx, float wy, float wz, int& sx, int& sy, float& depth) {
    const Vec3 c = to_camera(basis, Vec3{wx - eye_x, wy - eye_y, wz - eye_z});
    depth = c.z;
    return project(c, camera, image.width, image.height, sx, sy);
  };

  for (long z = cz - reach; z < cz + reach; ++z) {
    if (z < 0 || z >= side) continue;
    for (long x = cx - reach; x < cx + reach; ++x) {
      if (x < 0 || x >= side) continue;
      const float h[4] = {
          field.sample(static_cast<std::size_t>(x), static_cast<std::size_t>(z)),
          field.sample(static_cast<std::size_t>(x + 1), static_cast<std::size_t>(z)),
          field.sample(static_cast<std::size_t>(x + 1), static_cast<std::size_t>(z + 1)),
          field.sample(static_cast<std::size_t>(x), static_cast<std::size_t>(z + 1))};
      bool present = true;
      for (const float v : h) present = present && sample_is_present(v);
      if (!present) continue;

      const float wx = static_cast<float>(x) * step - kTerrainWorldBias;
      const float wz = static_cast<float>(z) * step - kTerrainWorldBias;
      const bool sea = water != nullptr
                           ? water->is_water(wx + step * 0.5F, wz + step * 0.5F)
                           : (h[0] <= kSeaLevelForShading && h[2] <= kSeaLevelForShading);

      // A lambert term from the quad's own slope, light from the north-west.
      const float nx = (h[0] - h[1]) / step, nz = (h[0] - h[3]) / step;
      const float inv = 1.0F / std::sqrt(nx * nx + nz * nz + 1.0F);
      float shade = (0.55F * nx + 0.35F * nz + 0.80F) * inv + 0.22F;
      shade = shade < 0.30F ? 0.30F : (shade > 1.25F ? 1.25F : shade);
      const int base_r = sea ? 44 : 96, base_g = sea ? 74 : 112, base_b = sea ? 116 : 72;

      int px[4], py[4];
      float pd[4];
      bool ok = true;
      const float corner[4][3] = {{wx, h[0], wz},
                                  {wx + step, h[1], wz},
                                  {wx + step, h[2], wz + step},
                                  {wx, h[3], wz + step}};
      for (int i = 0; i < 4 && ok; ++i) {
        ok = place(corner[i][0], corner[i][1], corner[i][2], px[i], py[i], pd[i]);
      }
      if (!ok) continue;
      const auto c = [&](int v) {
        const float lit = static_cast<float>(v) * shade;
        return static_cast<std::uint8_t>(lit > 255.0F ? 255.0F : lit);
      };
      image.triangle(px[0], py[0], pd[0], px[1], py[1], pd[1], px[2], py[2], pd[2],
                     c(base_r), c(base_g), c(base_b));
      image.triangle(px[0], py[0], pd[0], px[2], py[2], pd[2], px[3], py[3], pd[3],
                     c(base_r), c(base_g), c(base_b));
    }
  }
}

}  // namespace ac6::demo

namespace ac6::demo {

void draw_world_triangles(Image& image, const ac6::retail::RetailBasis& basis,
                          const DemoCamera& camera,
                          const ac6::retail::FlightPosition& position,
                          const std::vector<float>& xyz) noexcept {
  for (std::size_t t = 0; t + 8 < xyz.size(); t += 9) {
    const float* v = xyz.data() + t;
    int sx[3], sy[3];
    float depth[3];
    bool ok = true;
    for (int i = 0; i < 3 && ok; ++i) {
      const Vec3 c = to_camera(basis, Vec3{v[i * 3 + 0] - position.at64,
                                           v[i * 3 + 1] - position.at68,
                                           v[i * 3 + 2] - position.at72});
      depth[i] = c.z;
      ok = project(c, camera, image.width, image.height, sx[i], sy[i]);
    }
    if (!ok) continue;
    const float ax = v[3] - v[0], ay = v[4] - v[1], az = v[5] - v[2];
    const float bx = v[6] - v[0], by = v[7] - v[1], bz = v[8] - v[2];
    const float nx = ay * bz - az * by;
    const float ny = az * bx - ax * bz;
    const float nz = ax * by - ay * bx;
    const float len = std::sqrt(nx * nx + ny * ny + nz * nz) + 1e-6F;
    const float lit = 0.45F + 0.55F * std::fabs(ny / len);   // light, invented
    const auto c = [&](float k) {
      const float value = 205.0F * lit * k;
      return static_cast<std::uint8_t>(value > 255.0F ? 255.0F : value);
    };
    image.triangle(sx[0], sy[0], depth[0], sx[1], sy[1], depth[1],
                   sx[2], sy[2], depth[2], c(1.0F), c(0.96F), c(0.90F));
  }
}

}  // namespace ac6::demo

namespace ac6::demo {

void draw_world_triangles_textured(Image& image,
                                   const ac6::retail::RetailBasis& basis,
                                   const DemoCamera& camera,
                                   const ac6::retail::FlightPosition& position,
                                   const std::vector<float>& xyz,
                                   const std::vector<float>& uv,
                                   const std::uint32_t* texels, int tw, int th,
                                   const float sun[3], float fog_far,
                                   float fog_density,
                                   std::uint8_t fog_r, std::uint8_t fog_g,
                                   std::uint8_t fog_b) noexcept {
  const std::size_t triangles = xyz.size() / 9;
  for (std::size_t t = 0; t < triangles; ++t) {
    const float* v = xyz.data() + t * 9;
    const float* w = uv.data() + t * 6;
    int sx[3], sy[3];
    float depth[3];
    bool ok = true;
    for (int i = 0; i < 3 && ok; ++i) {
      const Vec3 c = to_camera(basis, Vec3{v[i * 3 + 0] - position.at64,
                                           v[i * 3 + 1] - position.at68,
                                           v[i * 3 + 2] - position.at72});
      depth[i] = c.z;
      ok = project(c, camera, image.width, image.height, sx[i], sy[i]);
    }
    if (!ok) continue;

    const float ax = v[3] - v[0], ay = v[4] - v[1], az = v[5] - v[2];
    const float bx = v[6] - v[0], by = v[7] - v[1], bz = v[8] - v[2];
    float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
    const float len = std::sqrt(nx * nx + ny * ny + nz * nz) + 1e-6F;
    nx /= len; ny /= len; nz /= len;
    float lit = 0.42F + 0.58F * std::fabs(nx * sun[0] + ny * sun[1] + nz * sun[2]);
    if (lit > 1.0F) lit = 1.0F;

    // The fog of .sky1: exponential in distance, saturating at fog.far.
    const float d = depth[0] > 0.0F ? depth[0] : 0.0F;
    float fog = 1.0F - std::exp(-fog_density * (d / (fog_far > 1.0F ? fog_far : 1.0F)) * 100.0F);
    if (fog < 0.0F) fog = 0.0F;
    if (fog > 1.0F) fog = 1.0F;

    if (texels != nullptr && tw > 0 && th > 0) {
      image.triangle_textured(sx[0], sy[0], depth[0], w[0], w[1],
                              sx[1], sy[1], depth[1], w[2], w[3],
                              sx[2], sy[2], depth[2], w[4], w[5],
                              texels, tw, th, lit * (1.0F - fog));
    } else {
      const auto c = [&](float base) {
        const float value = base * lit * (1.0F - fog);
        return static_cast<std::uint8_t>(value > 255.0F ? 255.0F : value);
      };
      image.triangle(sx[0], sy[0], depth[0], sx[1], sy[1], depth[1],
                     sx[2], sy[2], depth[2], c(200.0F), c(192.0F), c(180.0F));
    }
    (void)fog_r; (void)fog_g; (void)fog_b;
  }
}

}  // namespace ac6::demo

namespace ac6::demo {

void apply_mapset_post(Image& image, const MapsetPost& post) noexcept {
  const std::size_t pixels = static_cast<std::size_t>(image.width) * image.height;
  if (image.rgb.size() < pixels * 3) return;

  // 1. Bloom: a bright pass, blurred separably, added back. The threshold and
  //    scale are retail's; the kernel is mine.
  if (post.bloom) {
    std::vector<float> bright(pixels * 3, 0.0F);
    const float cut = post.bloom_threshold * 255.0F;
    for (std::size_t i = 0; i < pixels; ++i) {
      const float r = image.rgb[i * 3], g = image.rgb[i * 3 + 1], b = image.rgb[i * 3 + 2];
      const float luma = 0.299F * r + 0.587F * g + 0.114F * b;
      if (luma <= cut) continue;
      const float k = (luma - cut) / 255.0F;
      bright[i * 3] = r * k; bright[i * 3 + 1] = g * k; bright[i * 3 + 2] = b * k;
    }
    const int radius = static_cast<int>(post.bloom_sigma * 3.0F);
    std::vector<float> pass(pixels * 3, 0.0F);
    for (int axis = 0; axis < 2; ++axis) {
      std::vector<float>& src = axis == 0 ? bright : pass;
      std::vector<float>& dst = axis == 0 ? pass : bright;
      for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
          float acc[3] = {0.0F, 0.0F, 0.0F};
          float weight = 0.0F;
          for (int t = -radius; t <= radius; ++t) {
            const int sx = axis == 0 ? x + t : x;
            const int sy = axis == 0 ? y : y + t;
            if (sx < 0 || sy < 0 || sx >= image.width || sy >= image.height) continue;
            const float w = std::exp(-0.5F * static_cast<float>(t * t) /
                                     (post.bloom_sigma * post.bloom_sigma));
            const std::size_t o = (static_cast<std::size_t>(sy) * image.width + sx) * 3;
            acc[0] += src[o] * w; acc[1] += src[o + 1] * w; acc[2] += src[o + 2] * w;
            weight += w;
          }
          const std::size_t o = (static_cast<std::size_t>(y) * image.width + x) * 3;
          for (int c = 0; c < 3; ++c) dst[o + c] = weight > 0.0F ? acc[c] / weight : 0.0F;
        }
      }
    }
    for (std::size_t i = 0; i < pixels * 3; ++i) {
      const float v = image.rgb[i] + bright[i] * post.bloom_scale;
      image.rgb[i] = static_cast<std::uint8_t>(v > 255.0F ? 255.0F : v);
    }
  }

  // 2. Levels, per channel, exactly as .LevelCorrection states them.
  if (post.level_correction) {
    for (std::size_t i = 0; i < pixels; ++i) {
      for (int c = 0; c < 3; ++c) {
        const float span = post.in_max[c] - post.in_min[c];
        float t = span > 0.0F
                      ? (static_cast<float>(image.rgb[i * 3 + c]) - post.in_min[c]) / span
                      : 0.0F;
        t = t < 0.0F ? 0.0F : (t > 1.0F ? 1.0F : t);
        if (post.in_gamma[c] > 0.0F) t = std::pow(t, 1.0F / post.in_gamma[c]);
        const float v = post.out_min[c] + (post.out_max[c] - post.out_min[c]) * t;
        image.rgb[i * 3 + c] = static_cast<std::uint8_t>(v > 255.0F ? 255.0F : (v < 0.0F ? 0.0F : v));
      }
    }
  }

  // 3. Vignette, darkening beyond `fRadiusRatio` of the half-diagonal.
  if (post.vignette) {
    const float cx = image.width * 0.5F, cy = image.height * 0.5F;
    const float half = std::sqrt(cx * cx + cy * cy);
    for (int y = 0; y < image.height; ++y) {
      for (int x = 0; x < image.width; ++x) {
        const float dx = x - cx, dy = y - cy;
        const float r = std::sqrt(dx * dx + dy * dy) / half;
        if (r <= post.vignette_radius_ratio) continue;
        const float t = (r - post.vignette_radius_ratio) /
                        (1.0F - post.vignette_radius_ratio);
        const float k = 1.0F - 0.55F * t * t;          // the falloff shape is mine
        const std::size_t o = (static_cast<std::size_t>(y) * image.width + x) * 3;
        for (int c = 0; c < 3; ++c) {
          image.rgb[o + c] = static_cast<std::uint8_t>(image.rgb[o + c] * k);
        }
      }
    }
  }
}

}  // namespace ac6::demo
