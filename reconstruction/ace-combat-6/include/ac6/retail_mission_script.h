#pragma once

// The sub-mission script driver, ported from the retail chain that runs it.
//
// Cycle 1097 read the two halves of the script - 0x8226E908 selects a
// sub-mission, 0x8226E158 executes one step - and stopped there, because
// Mission 01 has no tag-7 step and therefore no counter condition to advance
// it. Cycle 1120 read the part that does advance it, and it is not in the
// script at all: it is a state of the mission machine.
//
//   0x822ED708, signal -2, at 0x822ED800:
//       bl 0x82267370            ; advance the script by one step
//       rlwinm r11,r3,0,0x18,0x1f
//       beq    0x822ED834        ; zero: dispatch the step that is now current
//       bl 0x82266710            ; non-zero: the script has run out
//
// So the mission advances one script step per frame of the in-mission state,
// and the mission ends when - and only when - the script runs out. There is no
// other terminator on this path: nothing in Mission 01's payload completes it.
//
// 0x82267370 itself, transcribed instruction by instruction:
//
//   if (ctx+0x268 || ctx+0x10 || ctx+0x14) ctx+0x14 += 1;   // 82267384-822673B0
//   count = *(u8*)*(sub_missions[ctx+0x10] + 8);            // 822673D4-822673EC
//   if (ctx+0x14 >= count) {                                // 822673F0 cmpw/blt
//     ctx+0x10 += 1; ctx+0x14 = 0;                          // 822673F8-82267404
//     if (ctx+0x10 >= FUN_82093DD0(ctx)) return 1;          // 82267408-82267418
//   }
//   return FUN_8226E158(ctx);                               // 82267434
//
// The first clause is why the entry call is not a special case: at state entry
// the cursor and the step pointer are all zero, so the very first advance runs
// step 0 of sub-mission 0 without incrementing anything. 0x82258D88 makes that
// call on its own entry branch (signal -3, at 0x8225912C).

#include "ac6/retail_scenario.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ac6::retail {

// What one advance did.
enum class ScriptAdvance : std::uint8_t {
  Ran,       // 0x82267370 returned 0: a step is current and was dispatched
  Exhausted, // it returned 1: the sub-mission index passed the parsed count
};

// One dispatched step, in the order 0x8226E158 reaches it.
struct ScriptStepRun {
  std::uint32_t sub_mission{};
  std::uint32_t step{};
  std::uint8_t tag{};
  bool operator==(const ScriptStepRun&) const = default;
};

// The tags of 0x8226E158's switch that fall through to the advance instead of
// doing work of their own. In retail this happens only under two runtime
// conditions - FUN_820F61B0(global+0x70) == 7, or global+0x78 == 4 - so it is a
// mode, not a property of the payload, and the caller states which mode it is
// running rather than having one assumed for it.
bool step_tag_delegates_to_advance(std::uint8_t tag) noexcept;

// The mission-end code 0x82266710 writes to context+0x338. Retail also tests
// this field against 2 at 0x82267868; that value has no producer on this path
// and is not modelled.
inline constexpr std::int32_t kMissionEndByScript = 1;

class MissionScriptRunner final {
 public:
  // The counts the loader takes from root slot 2: how many sub-missions, and
  // how many steps each one has. Both come from the payload; neither is a
  // native choice.
  static MissionScriptRunner from(const MissionScenario& scenario);

  // 0x82258D88's entry call. Identical to advance() - the guard in 0x82267370
  // is what makes the first one different - and named apart so the caller's
  // intent stays readable.
  ScriptAdvance start() noexcept { return advance(); }

  // One call of 0x82267370.
  ScriptAdvance advance() noexcept;

  // 0x822ED708 on signal -2: advance once, and on exhaustion take the branch at
  // 0x822ED810 into 0x82266710. Three runtime preconditions guard that call in
  // retail - context+0x820 non-zero, FUN_82268C58 non-null, and the low six bits
  // of its +0x124 clear - and none of them is modelled here: they gate on
  // objects the native session has no counterpart for, so this driver runs the
  // frame the way retail runs it once those gates are open.
  ScriptAdvance drive_frame() noexcept;

  // 0x82266710: context+0x338 = 1, then the manager's +0xB0 and +0xBC virtuals.
  // Called by the exhaustion branch above and by nothing else.
  void end_by_script() noexcept;

  // 0x8226E908: set the cursor, reset the step, and report exhaustion when the
  // index is at or past the parsed count. The tag-7 jump goes through here;
  // Mission 01 never takes it.
  ScriptAdvance select(std::uint32_t sub_mission) noexcept;

  // When true, tags 1, 4, 5, 6 and 8 chain into another advance the way they do
  // in mode 7. Off by default: the campaign path waits a frame per step.
  void set_auto_advance(bool enabled) noexcept { auto_advance_ = enabled; }

  std::uint32_t sub_mission() const noexcept { return sub_mission_; }
  std::uint32_t step() const noexcept { return step_; }
  bool step_current() const noexcept { return current_.has_value(); }
  std::optional<ScriptStepRun> current_step() const noexcept { return current_; }
  // The tag-7 payload attached to the current step, if it has one.  The
  // caller owns counter state and decides when a condition is evaluated.
  std::optional<ScenarioStepCondition> current_condition() const noexcept;
  const std::vector<ScriptStepRun>& executed() const noexcept { return executed_; }
  std::size_t sub_mission_count() const noexcept { return steps_.size(); }

  // context+0x338, and the only thing that writes it here is 0x82266710 on the
  // exhaustion branch of 0x822ED708. Zero while the mission is running.
  std::int32_t end_code() const noexcept { return end_code_; }
  bool ended() const noexcept { return end_code_ != 0; }

 private:
  // 0x8226E158's step dispatch, reduced to what the cursor needs: which tag is
  // current, and whether that tag hands control back to the advance.
  ScriptAdvance dispatch_step() noexcept;

  std::vector<std::vector<std::uint8_t>> steps_;  // tags, by sub-mission
  std::vector<std::vector<std::optional<ScenarioStepCondition>>> conditions_;
  std::vector<std::uint8_t> step_bounds_;         // the count byte per sub-mission
  std::vector<ScriptStepRun> executed_;
  std::optional<ScriptStepRun> current_;          // context+0x268
  std::uint32_t sub_mission_{};                   // context+0x10
  std::uint32_t step_{};                          // context+0x14
  std::int32_t end_code_{};                       // context+0x338
  bool auto_advance_{};
};

}  // namespace ac6::retail
