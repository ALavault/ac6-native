#include "ac6/retail_mission_script.h"

namespace ac6::retail {
namespace {

// 0x8226E158's switch: case 0 does the work of a sub-mission start, case 7 is
// the counter condition, and cases 1, 4, 5, 6 and 8 share one arm whose whole
// body is a call back into 0x82267370 - taken only when the mode test at
// 0x8226E7F4-0x8226E808 succeeds, that is when FUN_820F61B0(global+0x70) is 7
// or global+0x78 is 4. Cases 2, 3 and 9 are absent from Mission 01 and are not
// modelled; they fall through to "no delegation", which is what the default arm
// of the retail switch does as well.
constexpr bool delegating_tag(std::uint8_t tag) noexcept {
  return tag == 1 || tag == 4 || tag == 5 || tag == 6 || tag == 8;
}

}  // namespace

bool step_tag_delegates_to_advance(std::uint8_t tag) noexcept {
  return delegating_tag(tag);
}

MissionScriptRunner MissionScriptRunner::from(const MissionScenario& scenario) {
  MissionScriptRunner runner;
  for (const ScenarioSubMission& sub_mission : scenario.sub_missions()) {
    runner.steps_.push_back(sub_mission.step_tags);
    runner.step_bounds_.push_back(sub_mission.step_count_byte);
  }
  return runner;
}

// 0x82267370, in its own order.
ScriptAdvance MissionScriptRunner::advance() noexcept {
  // 82267384-822673A4: the three-way guard. All zero only at state entry, which
  // is why the first advance - the one 0x82258D88 makes on its own entry branch
  // at 0x8225912C - runs step 0 instead of skipping it.
  if (current_.has_value() || sub_mission_ != 0 || step_ != 0) {
    step_ += 1;  // 822673A8-822673B0
  }
  // 822673D4-822673EC: the count byte of the sub-mission the cursor is on.
  // Retail indexes the parsed array without checking it first; a cursor already
  // past the end would read whatever follows. Refuse instead, the way the unit
  // table refuses its 257th insertion.
  if (sub_mission_ >= step_bounds_.size()) {
    current_.reset();
    return ScriptAdvance::Exhausted;
  }
  // 822673F0: cmpw/blt, a signed comparison of the cursor against the byte.
  if (step_ >= step_bounds_[sub_mission_]) {
    sub_mission_ += 1;  // 822673F8-82267404
    step_ = 0;
    // 82267408-82267418: FUN_82093DD0 returns the sub-mission count byte of
    // root slot 2, and the cursor reaching it ends the script.
    if (sub_mission_ >= steps_.size()) {
      current_.reset();
      return ScriptAdvance::Exhausted;
    }
  }
  return dispatch_step();  // 82267434
}

// 0x8226E908.
ScriptAdvance MissionScriptRunner::select(std::uint32_t sub_mission) noexcept {
  sub_mission_ = sub_mission;
  step_ = 0;
  if (sub_mission >= steps_.size()) {
    current_.reset();
    return ScriptAdvance::Exhausted;
  }
  return dispatch_step();
}

// 0x8226E158, reduced to the cursor's view of it: publish the current step to
// context+0x268, then let the tag decide whether control returns to the advance.
ScriptAdvance MissionScriptRunner::dispatch_step() noexcept {
  // The bound the tag dispatch itself walks is the step table, not the count
  // byte; a count byte larger than the table would step off the end in retail.
  const std::vector<std::uint8_t>& tags = steps_[sub_mission_];
  if (step_ >= tags.size()) {
    current_.reset();
    return ScriptAdvance::Exhausted;
  }
  const ScriptStepRun run{sub_mission_, step_, tags[step_]};
  current_ = run;      // context+0x268
  executed_.push_back(run);
  if (auto_advance_ && delegating_tag(run.tag)) {
    // 0x8226E810: the delegating arm calls 0x82267370 again. The chain ends
    // because every call moves the cursor forward.
    return advance();
  }
  return ScriptAdvance::Ran;
}

// 0x822ED708 signal -2, at 0x822ED7FC-0x822ED814.
ScriptAdvance MissionScriptRunner::drive_frame() noexcept {
  const ScriptAdvance result = advance();
  if (result == ScriptAdvance::Exhausted) end_by_script();
  return result;
}

// 0x82266710: stw r10,0x338(r31) with r10 = 1, then the two virtual calls. The
// virtuals belong to the mission manager and are not this object's to make; the
// field is what the rest of the binary reads, and it is what is reproduced.
void MissionScriptRunner::end_by_script() noexcept {
  end_code_ = kMissionEndByScript;
}

}  // namespace ac6::retail
