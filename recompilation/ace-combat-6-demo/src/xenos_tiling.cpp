#include "ac6demo/xenos_tiling.hpp"

#include "ac6demo/runtime_error.hpp"

#include <algorithm>
#include <string>

namespace ac6demo {
namespace {

// Port of ReXGlue texture_util::GetTiledOffset2D, upstream commit
// cb58065c793429aa92895d778af58d12e9d26d8f (also present in the pinned
// SDK tree 741541d6035616dc406f7d74c2fe8f155913c77b). This is generic Xenos
// authority, not AC6 evidence.
std::int32_t tiled_offset_2d(std::int32_t x, std::int32_t y,
                            std::uint32_t pitch,
                            std::uint32_t bytes_per_pixel_log2) {
  pitch = (pitch + 31U) & ~31U;
  const std::int32_t macro =
      ((x >> 5) + (y >> 5) * static_cast<std::int32_t>(pitch >> 5))
      << (bytes_per_pixel_log2 + 7U);
  const std::int32_t micro =
      ((x & 7) + ((y & 0xEU) << 2)) << bytes_per_pixel_log2;
  const std::int32_t offset = macro + ((micro & ~0xF) << 1) +
                              (micro & 0xF) + ((y & 1) << 4);
  return ((offset & ~0x1FF) << 3) + ((y & 16) << 7) +
         ((offset & 0x1C0) << 2) +
         (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F);
}

void require_reached_buffer_sizes(std::size_t tiled,
                                  std::size_t linear,
                                  const char *operation) {
  if (tiled != kReachedResolveTiledExtentBytes ||
      linear != kReachedResolveLinearBytes) {
    throw RuntimeTrap(std::string("reached RGBA8 ") + operation +
                      " buffer size mismatch");
  }
}

} // namespace

std::size_t reached_rgba8_tiled_offset(std::uint32_t x, std::uint32_t y) {
  if (x >= kReachedResolveWidth || y >= kReachedResolveHeight) {
    throw RuntimeTrap("reached RGBA8 tiled coordinate is out of bounds");
  }
  const auto offset = tiled_offset_2d(static_cast<std::int32_t>(x),
                                      static_cast<std::int32_t>(y),
                                      kReachedResolvePitch, 2U);
  if (offset < 0 || static_cast<std::size_t>(offset) + 4U >
                        kReachedResolveTiledExtentBytes) {
    throw RuntimeTrap("reached RGBA8 tiled offset exceeds qualified extent");
  }
  return static_cast<std::size_t>(offset);
}

void tile_reached_rgba8(std::span<const std::byte> linear,
                        std::span<std::byte> tiled) {
  require_reached_buffer_sizes(tiled.size(), linear.size(), "tile");
  for (std::uint32_t y = 0; y < kReachedResolveHeight; ++y) {
    for (std::uint32_t x = 0; x < kReachedResolveWidth; ++x) {
      const std::size_t source =
          (static_cast<std::size_t>(y) * kReachedResolveWidth + x) * 4U;
      const std::size_t destination = reached_rgba8_tiled_offset(x, y);
      std::copy_n(linear.begin() + static_cast<std::ptrdiff_t>(source), 4U,
                  tiled.begin() + static_cast<std::ptrdiff_t>(destination));
    }
  }
}

void untile_reached_rgba8(std::span<const std::byte> tiled,
                          std::span<std::byte> linear) {
  require_reached_buffer_sizes(tiled.size(), linear.size(), "untile");
  for (std::uint32_t y = 0; y < kReachedResolveHeight; ++y) {
    for (std::uint32_t x = 0; x < kReachedResolveWidth; ++x) {
      const std::size_t source = reached_rgba8_tiled_offset(x, y);
      const std::size_t destination =
          (static_cast<std::size_t>(y) * kReachedResolveWidth + x) * 4U;
      std::copy_n(tiled.begin() + static_cast<std::ptrdiff_t>(source), 4U,
                  linear.begin() + static_cast<std::ptrdiff_t>(destination));
    }
  }
}

} // namespace ac6demo
