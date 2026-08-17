#include "ac6/execution_trace.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace ac6::retail_cli::detail {

bool append_native_replay_trace_sample(ExecutionTraceJsonlWriter& writer,
                                       std::uint64_t frame_index,
                                       InputFrame input,
                                       const WorldFrame& simulation,
                                       TraceMissionObjectives mission,
                                       TraceGraphicsSubmission graphics);

}  // namespace ac6::retail_cli::detail

namespace {

int check(bool condition, const char* message) {
  if (!condition) std::cerr << "FAIL " << message << '\n';
  return condition ? 0 : 1;
}

ac6::WorldFrame frame(std::uint64_t tick, ac6::InputFrame input) {
  ac6::WorldFrame result;
  result.tick = tick;
  result.mission_id = 1;
  result.mission_ready = true;
  result.input = input;
  return result;
}

}  // namespace

int main() {
  const char* path = "ac6-test-retail-replay-trace-60hz.raw.jsonl";
  int failures = 0;
  ac6::ExecutionTraceJsonlWriter writer;
  failures += check(writer.open(path), "trace opens");

  // Three deliberately distinct frames prove that no even-count or
  // duplicated-pair precondition remains between the 60 Hz replay and trace
  // domains.
  constexpr std::array<ac6::InputFrame, 3> inputs{{
      {100, -200, 300, 10, 0x0001},
      {-400, 500, -600, 20, 0x0002},
      {700, -800, 900, 30, 0x0004},
  }};
  for (std::size_t index = 0; index < inputs.size(); ++index) {
    failures += check(
        ac6::retail_cli::detail::append_native_replay_trace_sample(
            writer, static_cast<std::uint64_t>(index), inputs[index],
            frame(index + 1u, inputs[index]),
            {ac6::ScenarioState::Gameplay, 0, static_cast<std::uint32_t>(index),
             false, {}},
            {ac6::TraceGraphicsBackend::Headless, 0, false}),
        "each native frame writes one trace sample");
  }
  failures += check(writer.event_count() == inputs.size() * 6u,
                    "three native frames write eighteen domain events");
  failures += check(!ac6::retail_cli::detail::append_native_replay_trace_sample(
                        writer, std::numeric_limits<std::uint64_t>::max(),
                        inputs.back(), frame(4, inputs.back()), {}, {}),
                    "sample tick overflow fails closed");
  failures += check(writer.event_count() == inputs.size() * 6u,
                    "rejected overflow writes no event");
  failures += check(writer.close(), "trace closes");

  std::ifstream input(path);
  std::vector<std::string> lines;
  for (std::string line; std::getline(input, line);)
    lines.push_back(line);
  failures += check(lines.size() == inputs.size() * 6u,
                    "all domain events persist");
  for (std::size_t index = 0; index < inputs.size(); ++index) {
    const std::string tick = "\"tick\":" + std::to_string(index + 1u);
    const std::string pitch =
        "\"pitch\":" + std::to_string(inputs[index].pitch);
    failures += check(lines[index * 6u].find(tick) != std::string::npos &&
                          lines[index * 6u].find(pitch) != std::string::npos,
                      "each controller event preserves its frame index and input");
  }
  std::remove(path);
  return failures == 0 ? 0 : 1;
}
