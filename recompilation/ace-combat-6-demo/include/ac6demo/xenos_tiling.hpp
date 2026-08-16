#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace ac6demo {

// Xenia/ReXGlue-generic 2D Xenos tiling, restricted to the only destination
// configuration reached by the qualified PAL demo resolve.
inline constexpr std::uint32_t kReachedResolveWidth = 1280U;
inline constexpr std::uint32_t kReachedResolveHeight = 720U;
inline constexpr std::uint32_t kReachedResolvePitch = 1280U;
inline constexpr std::size_t kReachedResolveLinearBytes =
    static_cast<std::size_t>(kReachedResolveWidth) * kReachedResolveHeight * 4U;
inline constexpr std::size_t kReachedResolveTiledExtentBytes = 0x398000U;

[[nodiscard]] std::size_t reached_rgba8_tiled_offset(std::uint32_t x,
                                                      std::uint32_t y);

void untile_reached_rgba8(std::span<const std::byte> tiled,
                          std::span<std::byte> linear);

} // namespace ac6demo
