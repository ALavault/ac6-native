#pragma once

#include <cstdint>
#include <string_view>

namespace ac6::display {

// AC6 presents a 1280x720 frontbuffer. Keep the guest-reported video mode and
// the native startup window aligned with it so UI coordinates are not laid out
// for 1080p and then clipped by the 720p acceptance display.
inline constexpr int32_t kWidth = 1280;
inline constexpr int32_t kHeight = 720;
inline constexpr std::string_view kResolutionPreset = "720p";

}  // namespace ac6::display
