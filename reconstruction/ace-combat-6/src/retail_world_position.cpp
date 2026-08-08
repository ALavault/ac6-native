#include "ac6/retail_world_position.h"

#include <cmath>

namespace ac6::retail {

// 0x820936E8, which 0x82093808 calls with the object's +0x30 and +0x38: below
// its epsilon on both it answers a constant instead of calling atan2 on two
// zeros. The constant is DAT_820542B8 and its value is not established here, so
// the degenerate case returns zero and says so rather than pretending.
float anchor_heading(float forward_x, float forward_z) noexcept {
  constexpr float kEpsilon = 1.0e-8f;  // DAT_82069C2C, order of magnitude only
  if (std::fabs(forward_x) < kEpsilon && std::fabs(forward_z) < kEpsilon) return 0.0f;
  return std::atan2(forward_x, forward_z);
}

// 0x822953F0.
std::optional<ScenarioVector> resolve_world_position(
    const ScenarioPositionRecord& record, const AnchorFrame* anchor,
    const float* queried_height) noexcept {
  // 82295410-82295434: the triple is loaded into the out vector before anything
  // else, so it is the answer on every path the transform does not take.
  ScenarioVector result{record.x, record.y, record.z};

  // 82295414-82295438: only mode 1 reaches the anchor lookup.
  if (record.mode == 1) {
    if (anchor == nullptr) return std::nullopt;
    // 82295478-82295488: the anchor's heading, then its sine and cosine. The
    // caller of the sincos pair stores the negation of the first slot, and the
    // middle row of the matrix is the constant (0,1,0), so the rotation is
    // about the vertical axis and the negated term is the sine.
    //
    // Which of the two off-diagonal signs is negative follows from the heading
    // itself rather than from the permute lanes: 0x82093808 computes
    // atan2(forward.x, forward.z), so a unit's forward direction is
    // (sin h, ., cos h), and a purely forward offset (0, 0, d) must resolve to
    // d * (sin h, 0, cos h). Only this arrangement does that.
    const float sine = std::sin(anchor->heading);
    const float cosine = std::cos(anchor->heading);
    const float x = cosine * record.x + sine * record.z;
    const float z = -sine * record.x + cosine * record.z;
    // 82295534-82295558: the rotated triple is added to the anchor's +0x40,
    // +0x44 and +0x48, component by component, y included and unrotated.
    result = {anchor->position.x + x, anchor->position.y + record.y,
              anchor->position.z + z};
  }

  // 8229555C-822955CC: bit 0 of the halfword at +0x40 replaces the y with what
  // a virtual on the object at global+0x36084 returns, plus the record's y.
  if ((record.flags & kPositionHeightFromQuery) != 0) {
    if (queried_height == nullptr) return std::nullopt;
    result.y = *queried_height + record.y;
  }
  return result;
}

}  // namespace ac6::retail
