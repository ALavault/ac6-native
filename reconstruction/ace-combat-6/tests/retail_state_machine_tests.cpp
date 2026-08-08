// The ported mission state machine.
//
// What is worth checking is not that a transition "works" but that it performs
// the steps 0x8219AD20 performs, in its order: exit up the current branch
// nearest first, then enter down the target branch outermost first, with
// nothing run on the common ancestor itself. A test that only compared the
// final state would pass on a machine that ran no handler at all.
//
// The tree's own invariants are re-measured here from the ported table, so a
// table edited by hand cannot quietly contradict cycle 1112.
//
// usage: retail-state-machine-tests [REPORT]

#include "ac6/retail_state_machine.h"
#include "test_fixtures.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>

namespace {

using ac6::retail::MissionState;
using ac6::retail::StateSignal;
using ac6::retail::StateStep;

std::size_t depth_of(std::uint32_t handler) {
  return ac6::retail::state_ancestry(handler).size() - 1;
}

}  // namespace

int main(int argc, char** argv) {
  const std::vector<MissionState>& states = ac6::retail::mission_states();
  REQUIRE(states.size() == 36);

  // One root, and it is the handler the linker never recorded as a function.
  std::size_t roots = 0;
  for (const MissionState& state : states) {
    if (state.superstate == 0) roots += 1;
  }
  REQUIRE(roots == 0);  // every listed state has a superstate; the root is not listed

  // Every ancestry terminates at the root, so the relation is acyclic with a
  // single root - the control that killed cycle 1111's first rule.
  std::size_t deepest = 0;
  for (const MissionState& state : states) {
    const std::vector<std::uint32_t> chain = ac6::retail::state_ancestry(state.handler);
    REQUIRE(!chain.empty());
    REQUIRE(chain.front() == state.handler);
    REQUIRE(chain.back() == ac6::retail::kMissionStateRoot);
    std::set<std::uint32_t> seen(chain.begin(), chain.end());
    REQUIRE(seen.size() == chain.size());  // no repeat: no cycle
    deepest = std::max(deepest, chain.size() - 1);
  }
  REQUIRE(deepest == 6);

  // The one entry code read directly from its branch, in cycle 1112.
  REQUIRE(ac6::retail::state_code(0x822E7760u) == 8);
  REQUIRE(ac6::retail::state_code(0x822E8660u) == 3);
  REQUIRE(ac6::retail::state_code(0x822EACC0u) == 7);

  // Siblings: one exit, one entry, nothing on the shared parent.
  const std::vector<StateStep> siblings =
      ac6::retail::plan_transition(0x822EACC0u, 0x822EB090u);
  REQUIRE(siblings.size() == 2);
  REQUIRE(siblings[0].signal == StateSignal::Exit && siblings[0].handler == 0x822EACC0u);
  REQUIRE(siblings[1].signal == StateSignal::Entry && siblings[1].handler == 0x822EB090u);

  // Two levels down each side of a shared grandparent: exits nearest first,
  // entries outermost first.
  const std::vector<StateStep> deep =
      ac6::retail::plan_transition(0x822E6E98u, 0x822E6CC0u);
  REQUIRE(deep.size() == 4);
  REQUIRE(deep[0].signal == StateSignal::Exit && deep[0].handler == 0x822E6E98u);
  REQUIRE(deep[1].signal == StateSignal::Exit && deep[1].handler == 0x822EACC0u);
  REQUIRE(deep[2].signal == StateSignal::Entry && deep[2].handler == 0x822ED070u);
  REQUIRE(deep[3].signal == StateSignal::Entry && deep[3].handler == 0x822E6CC0u);

  // Into a descendant: no exit at all, one entry per level crossed.
  const std::vector<StateStep> downward =
      ac6::retail::plan_transition(0x822ED708u, 0x822E6E98u);
  REQUIRE(downward.size() == 2);
  REQUIRE(downward[0].signal == StateSignal::Entry && downward[0].handler == 0x822EACC0u);
  REQUIRE(downward[1].signal == StateSignal::Entry && downward[1].handler == 0x822E6E98u);

  // Out to an ancestor: exits only.
  const std::vector<StateStep> upward =
      ac6::retail::plan_transition(0x822E6E98u, 0x822ED708u);
  REQUIRE(upward.size() == 2);
  REQUIRE(upward[0].signal == StateSignal::Exit && upward[0].handler == 0x822E6E98u);
  REQUIRE(upward[1].signal == StateSignal::Exit && upward[1].handler == 0x822EACC0u);

  // A state to itself moves nothing.
  REQUIRE(ac6::retail::plan_transition(0x822E7760u, 0x822E7760u).empty());
  // An unknown handler yields no plan rather than a guess.
  REQUIRE(ac6::retail::plan_transition(0x11111111u, 0x822E7760u).empty());

  std::size_t with_code = 0;
  for (const MissionState& state : states) {
    if (ac6::retail::state_code(state.handler).has_value()) with_code += 1;
  }

  if (argc >= 2) {
    std::ofstream report(argv[1]);
    REQUIRE(static_cast<bool>(report));
    report << "{\n"
           << "  \"schema\": \"ac6.retail-state-machine.v1\",\n"
           << "  \"states\": " << states.size() << ",\n"
           << "  \"root\": \"0x822E39A8\",\n"
           << "  \"acyclic\": true,\n"
           << "  \"max_depth\": " << deepest << ",\n"
           << "  \"states_with_attributed_code\": " << with_code << ",\n"
           << "  \"state_code_0x822E7760\": 8,\n"
           << "  \"transition_order\": \"exit nearest first, entry outermost first, "
              "nothing on the common ancestor\"\n"
           << "}\n";
    REQUIRE(static_cast<bool>(report));
  }
  std::printf("retail_state_machine states=%zu depth=%zu coded=%zu\n", states.size(),
              deepest, with_code);
  return 0;
}
