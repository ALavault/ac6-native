#include "ac6demo/runtime_error.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

namespace {

struct Vertex {
  float x;
  float y;
  std::array<float, 4> color;
};

struct Point {
  float x;
  float y;
  bool operator==(const Point &) const = default;
};

using Pixel = std::array<std::uint8_t, 4>;

class ReachedRectangleRasterizer final {
public:
  ReachedRectangleRasterizer(std::uint32_t width, std::uint32_t height)
      : width_(width), height_(height), pixels_(width * height),
        coverage_(width * height), known_(width * height) {
    if (width != 1280U || height != 720U) {
      throw ac6demo::RuntimeTrap(
          "test-only rasterizer admits only reached 1280x720 viewport");
    }
  }

  void cover_reached_resolve(const std::array<Point, 3> &vertices) {
    if (vertices != std::array<Point, 3>{{
                        {-0.5F, -0.5F},
                        {1279.5F, -0.5F},
                        {1279.5F, 719.5F},
                    }}) {
      throw ac6demo::RuntimeTrap("unqualified reached resolve rectangle");
    }
    std::fill(coverage_.begin(), coverage_.end(), std::uint8_t{1});
  }

  void fill(Pixel value) { std::fill(pixels_.begin(), pixels_.end(), value); }

  void draw_reached_rectangle(const std::array<Vertex, 3> &vertices,
                              std::uint32_t color_mask) {
    if (color_mask != 0xFFFFU) {
      throw ac6demo::RuntimeTrap(
          "test-only rasterizer admits only reached full color mask");
    }
    if (vertices[0].x != -0.5F || vertices[0].y != -0.5F ||
        vertices[1].y != -0.5F || vertices[1].x != vertices[2].x ||
        vertices[2].y < -0.5F) {
      throw ac6demo::RuntimeTrap("unqualified reached rectangle geometry");
    }
    const auto right = static_cast<std::uint32_t>(vertices[1].x + 0.5F);
    const auto bottom = static_cast<std::uint32_t>(vertices[2].y + 0.5F);
    if (right > width_ || bottom > height_ || right == 0 || bottom == 0) {
      throw ac6demo::RuntimeTrap("reached rectangle exceeds viewport");
    }
    for (std::uint32_t y = 0; y < bottom; ++y) {
      const float fy = bottom == 1U ? 0.0F : static_cast<float>(y) /
                                                  static_cast<float>(bottom - 1U);
      for (std::uint32_t x = 0; x < right; ++x) {
        const float fx = right == 1U ? 0.0F : static_cast<float>(x) /
                                                static_cast<float>(right - 1U);
        Pixel result{};
        for (std::size_t channel = 0; channel < result.size(); ++channel) {
          const float top = vertices[0].color[channel] * (1.0F - fx) +
                            vertices[1].color[channel] * fx;
          const float bottom_color = vertices[0].color[channel] * (1.0F - fx) +
                                     vertices[2].color[channel] * fx;
          const float value = top * (1.0F - fy) + bottom_color * fy;
          if (value < 0.0F || value > 1.0F) {
            throw ac6demo::RuntimeTrap("unqualified unclamped rectangle color");
          }
          result[channel] = static_cast<std::uint8_t>(value * 255.0F + 0.5F);
        }
        pixels_[static_cast<std::size_t>(y) * width_ + x] = result;
        known_[static_cast<std::size_t>(y) * width_ + x] = 1U;
      }
    }
  }

  [[nodiscard]] const Pixel &at(std::uint32_t x, std::uint32_t y) const {
    return pixels_.at(static_cast<std::size_t>(y) * width_ + x);
  }

  [[nodiscard]] std::size_t known_pixel_count() const {
    return static_cast<std::size_t>(
        std::ranges::count(known_, std::uint8_t{1}));
  }

  [[nodiscard]] std::size_t known_edram_sample_count_4x() const {
    return known_pixel_count() * 4U;
  }

  [[nodiscard]] std::vector<Pixel> logical_resolve_rgba8(
      std::span<const Pixel> source,
      std::uint32_t source_pitch, std::uint32_t destination_pitch,
      std::uint32_t width, std::uint32_t height, std::uint32_t endian,
      bool destination_tiled) const {
    if (source_pitch != 1280U || destination_pitch != 1280U ||
        width != 1280U || height != 720U || endian != 0U ||
        !destination_tiled || source.size() != pixels_.size() ||
        std::ranges::find(coverage_, std::uint8_t{0}) != coverage_.end()) {
      throw ac6demo::RuntimeTrap("unqualified reached resolve state");
    }
    // This is deliberately the logical pre-tiling resolve oracle. The Vulkan
    // path must still prove and implement the Xenos tiled destination layout.
    return {source.begin(), source.end()};
  }

private:
  std::uint32_t width_;
  std::uint32_t height_;
  std::vector<Pixel> pixels_;
  std::vector<std::uint8_t> coverage_;
  std::vector<std::uint8_t> known_;
};

void test_normal_rectangle_and_logical_resolve() {
  ReachedRectangleRasterizer rasterizer(1280U, 720U);
  rasterizer.fill({0x11U, 0x22U, 0x33U, 0x44U});
  const std::array<Vertex, 3> normal{{
      {-0.5F, -0.5F, {0.0F, 0.0F, 0.0F, 0.0F}},
      {639.5F, -0.5F, {0.0F, 0.0F, 0.0F, 0.0F}},
      {639.5F, 359.5F, {0.0F, 0.0F, 0.0F, 0.0F}},
  }};
  rasterizer.draw_reached_rectangle(normal, 0xFFFFU);
  assert((rasterizer.at(0U, 0U) == Pixel{0U, 0U, 0U, 0U}));
  assert((rasterizer.at(639U, 359U) == Pixel{0U, 0U, 0U, 0U}));
  assert((rasterizer.at(640U, 359U) == Pixel{0x11U, 0x22U, 0x33U, 0x44U}));
  assert((rasterizer.at(0U, 360U) == Pixel{0x11U, 0x22U, 0x33U, 0x44U}));
  assert(rasterizer.known_pixel_count() == 640U * 360U);
  assert(rasterizer.known_edram_sample_count_4x() == 1280U * 720U);

  const std::vector<Pixel> source(1280U * 720U,
                                  Pixel{0xA1U, 0xB2U, 0xC3U, 0xD4U});
  const std::array<Point, 3> resolve{{
      {-0.5F, -0.5F}, {1279.5F, -0.5F}, {1279.5F, 719.5F}}};
  rasterizer.cover_reached_resolve(resolve);
  const auto resolved = rasterizer.logical_resolve_rgba8(
      source,
      1280U, 1280U, 1280U, 720U, 0U, true);
  assert(resolved.size() == 1280U * 720U);
  assert(resolved.front() == source.front());
  assert(resolved.back() == source.back());
}

void test_resolve_rectangle_and_fail_closed_state() {
  ReachedRectangleRasterizer rasterizer(1280U, 720U);
  const std::array<Point, 3> resolve{{
      {-0.5F, -0.5F}, {1279.5F, -0.5F}, {1279.5F, 719.5F}}};
  rasterizer.cover_reached_resolve(resolve);
  const std::vector<Pixel> source(1280U * 720U);

  bool trapped = false;
  try {
    static_cast<void>(rasterizer.logical_resolve_rgba8(
        source,
        1280U, 1280U, 1280U, 720U, 2U, true));
  } catch (const ac6demo::RuntimeTrap &) {
    trapped = true;
  }
  assert(trapped);
}

} // namespace

int main() {
  test_normal_rectangle_and_logical_resolve();
  test_resolve_rectangle_and_fail_closed_state();
  std::cout << "ac6-demo-reached-raster-tests: ok\n";
}
