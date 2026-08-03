#pragma once

#include <cstdint>
#include <string_view>

struct PPCContext;

namespace ac6::dialog_text {

inline constexpr std::string_view kMissingGameDataKey = "M70000_122";
inline constexpr std::string_view kMissingGameDataText =
    "No ACE COMBAT 6 Game Data.\nCreate new Game Data?";

struct FallbackLayout {
  float font_size;
  float first_line_y;
  float line_spacing;
};

// PAL oracle coordinates are relative to the 1280x720 game viewport (the
// oracle window's menu bar is outside it). Keep the fallback in the body area
// above the guest-rendered buttons when the host viewport is resized.
constexpr FallbackLayout LayoutForHeight(float display_height) {
  constexpr float kReferenceHeight = 720.0f;
  const float scale = display_height > 0.0f ? display_height / kReferenceHeight
                                            : 1.0f;
  return {
      28.0f * scale,
      278.0f * scale,
      34.0f * scale,
  };
}

constexpr std::string_view ReplacementFor(std::string_view key) {
  return key == kMissingGameDataKey ? kMissingGameDataText : std::string_view{};
}

// Marks a frame containing the PAL catalog key whose glyph record is not
// displayable by the active guest font atlas. All guest arguments remain
// untouched.
void ApplyFallback(PPCContext& ctx, uint8_t* base);

// The native presentation layer uses this short-lived signal to supply only
// the missing body text while the guest continues to draw the panel/buttons.
bool ShouldDrawFallback();

}  // namespace ac6::dialog_text
