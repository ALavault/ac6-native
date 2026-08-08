#pragma once

// The mission state machine, derived from the retail binary.
//
// Cycle 1109 read the transition engine 0x8219AD20 - two ancestor chains walked
// through member pointers, a common-ancestor search, an exit pass up one branch
// with signal -1, an entry pass down the other with -3, then a descent into
// initial substates with -5. Cycle 1114 named it: ACE6::Util::CHsm, a base at
// offset 0x348 of CX360MissionManager<ACE6::CAce6MissionManagerCampaign>.
// Cycle 1112 mapped the tree, 36 states rooted at 0x822E39A8, depth 6.
//
// This header ports the algorithm and the topology. The topology is compiled in
// rather than loaded, because it is code structure recovered from the binary,
// not mission content: the payload has no say in it.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace ac6::retail {

// The signal alphabet, read at its positions in 0x8219AD20 and confirmed on the
// handler 0x822E8660 in cycle 1110. Signal -2 exists and is not modelled.
enum class StateSignal : std::int32_t {
  Exit = -1,
  Entry = -3,
  Superstate = -4,
  Initial = -5,
};

// One state of the machine: its retail handler address, its superstate, and the
// code it publishes to context+0x260 on entry - or none where cycle 1112 could
// not attribute one by branch.
struct MissionState {
  std::uint32_t handler{};
  std::uint32_t superstate{};   // 0x822E39A8 is the root and has none
  std::int32_t entry_code{-1};  // -1 when unattributed
};

// The whole tree. Nothing here is inferred at run time.
const std::vector<MissionState>& mission_states();

// The root 0x822E39A8, which returns a null superstate.
inline constexpr std::uint32_t kMissionStateRoot = 0x822E39A8u;

// One step of a transition, in the order 0x8219AD20 performs them.
struct StateStep {
  StateSignal signal{};
  std::uint32_t handler{};
};

// 0x8219AD20: exit from the current state up to the common ancestor, enter down
// to the target, then descend into initial substates. Returns the exact sequence
// of handler invocations, so a test can check the order rather than the result.
// Empty when either state is unknown.
std::vector<StateStep> plan_transition(std::uint32_t current, std::uint32_t target);

// The state code a machine publishes after arriving at `handler`, or none when
// that state's code was never attributed by branch.
std::optional<std::int32_t> state_code(std::uint32_t handler);

// The chain from a state up to the root, the state itself first.
std::vector<std::uint32_t> state_ancestry(std::uint32_t handler);

}  // namespace ac6::retail
