#include "ac6/retail_state_machine.h"

#include <algorithm>
#include <map>

namespace ac6::retail {
namespace {

// The tree of cycle 1112: handler, superstate, entry code. The superstate is
// the address each handler materialises on its fall-through path, after every
// signal comparison missed; the relation is acyclic with a single root.
const std::vector<MissionState>& table() {
  static const std::vector<MissionState> states = {
    {0x822E4A48u, 0x822E39A8u, 4},  // depth 1
    {0x822E5280u, 0x822E39A8u, 10},  // depth 1
    {0x822E6870u, 0x822E9DF8u, -1},  // depth 4
    {0x822E6AB8u, 0x822EB090u, -1},  // depth 3
    {0x822E6CC0u, 0x822ED070u, -1},  // depth 3
    {0x822E6E98u, 0x822EACC0u, -1},  // depth 3
    {0x822E70B0u, 0x822ED708u, -1},  // depth 2
    {0x822E73B0u, 0x822E8660u, -1},  // depth 2
    {0x822E7598u, 0x822E39A8u, -1},  // depth 1
    {0x822E7760u, 0x822ED708u, 8},  // depth 2
    {0x822E7AF8u, 0x822E9458u, -1},  // depth 5
    {0x822E7E68u, 0x822E7AF8u, -1},  // depth 6
    {0x822E80A0u, 0x822EA648u, -1},  // depth 4
    {0x822E8660u, 0x822E39A8u, 3},  // depth 1
    {0x822E8A90u, 0x822E7AF8u, -1},  // depth 6
    {0x822E8CD8u, 0x822EA978u, -1},  // depth 4
    {0x822E8F80u, 0x822EA648u, -1},  // depth 4
    {0x822E9458u, 0x822E9DF8u, -1},  // depth 4
    {0x822E9720u, 0x822EA978u, -1},  // depth 4
    {0x822E9A30u, 0x822EA648u, -1},  // depth 4
    {0x822E9DF8u, 0x822EB5A8u, -1},  // depth 3
    {0x822EA220u, 0x822EA978u, -1},  // depth 4
    {0x822EA648u, 0x822EACC0u, -1},  // depth 3
    {0x822EA978u, 0x822EB090u, -1},  // depth 3
    {0x822EACC0u, 0x822ED708u, 7},  // depth 2
    {0x822EB090u, 0x822ED708u, 6},  // depth 2
    {0x822EB5A8u, 0x822ED708u, 2},  // depth 2
    {0x822EBD10u, 0x822EB5A8u, -1},  // depth 3
    {0x822EC090u, 0x822ECD90u, -1},  // depth 4
    {0x822EC4D0u, 0x822ECD90u, -1},  // depth 4
    {0x822EC998u, 0x822ECD90u, -1},  // depth 4
    {0x822ECD90u, 0x822ED070u, -1},  // depth 3
    {0x822ED070u, 0x822ED708u, 5},  // depth 2
    {0x822ED708u, 0x822E39A8u, 1},  // depth 1
    {0x822EDC58u, 0x822E39A8u, 1},  // depth 1
    {0x822EE5C0u, 0x822E39A8u, 1},  // depth 1
  };
  return states;
}

const std::map<std::uint32_t, MissionState>& index() {
  static const std::map<std::uint32_t, MissionState> built = [] {
    std::map<std::uint32_t, MissionState> map;
    for (const MissionState& state : table()) map.emplace(state.handler, state);
    return map;
  }();
  return built;
}

}  // namespace

const std::vector<MissionState>& mission_states() { return table(); }

std::vector<std::uint32_t> state_ancestry(std::uint32_t handler) {
  std::vector<std::uint32_t> chain;
  std::uint32_t cursor = handler;
  while (cursor != 0) {
    chain.push_back(cursor);
    if (cursor == kMissionStateRoot) break;
    const auto found = index().find(cursor);
    if (found == index().end()) break;
    cursor = found->second.superstate;
    // The tree is acyclic, but a malformed table must not hang the product.
    if (chain.size() > table().size() + 1) break;
  }
  return chain;
}

std::optional<std::int32_t> state_code(std::uint32_t handler) {
  const auto found = index().find(handler);
  if (found == index().end() || found->second.entry_code < 0) return std::nullopt;
  return found->second.entry_code;
}

std::vector<StateStep> plan_transition(std::uint32_t current, std::uint32_t target) {
  if (index().find(current) == index().end() && current != kMissionStateRoot) return {};
  if (index().find(target) == index().end() && target != kMissionStateRoot) return {};

  const std::vector<std::uint32_t> from = state_ancestry(current);
  const std::vector<std::uint32_t> to = state_ancestry(target);

  // The common ancestor: the deepest state both chains contain. 0x8219AD20
  // finds it by comparing the two chains entry by entry from the top.
  std::uint32_t common = 0;
  for (const std::uint32_t candidate : from) {
    if (std::find(to.begin(), to.end(), candidate) != to.end()) {
      common = candidate;
      break;
    }
  }

  std::vector<StateStep> steps;
  // Exit up the current branch, nearest first, stopping below the ancestor.
  for (const std::uint32_t state : from) {
    if (state == common) break;
    steps.push_back({StateSignal::Exit, state});
  }
  // Enter down the target branch, outermost first - the reverse of its chain.
  std::vector<std::uint32_t> entry;
  for (const std::uint32_t state : to) {
    if (state == common) break;
    entry.push_back(state);
  }
  for (auto it = entry.rbegin(); it != entry.rend(); ++it) {
    steps.push_back({StateSignal::Entry, *it});
  }
  return steps;
}

}  // namespace ac6::retail
