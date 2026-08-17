#include "ac6/execution_trace.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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
  result.position_x = 1.0f;
  result.position_y = 2.0f;
  result.position_z = 3.0f;
  result.player_entity = 7;
  result.active_units = 4;
  result.input = input;
  return result;
}

ac6::TraceMissionObjectives mission(std::uint32_t step) {
  ac6::TraceMissionObjectives result;
  result.state = ac6::ScenarioState::Gameplay;
  result.sub_mission = 1;
  result.step = step;
  result.objectives = {
      {2, "second", true, ac6::ObjectiveState::Pending},
      {1, "first", true, ac6::ObjectiveState::Active},
  };
  return result;
}

}  // namespace

int main() {
  const char* path = "ac6-test-execution-trace-v3.raw.jsonl";
  int failures = 0;
  ac6::ExecutionTraceJsonlWriter writer;
  failures += check(writer.open(path), "trace opens");
  const ac6::InputFrame first_input{1200, -2300, 3400, 200, 0x10};
  failures += check(writer.append(1, first_input, frame(2, first_input), mission(1),
                                  {ac6::TraceGraphicsBackend::Headless, 0, false}),
                    "first tick writes six domains");
  failures += check(!writer.append(3, first_input, frame(6, first_input), mission(3), {}),
                    "non-contiguous tick is rejected");
  const ac6::InputFrame second_input{};
  failures += check(writer.append(2, second_input, frame(4, second_input), mission(2),
                                  {ac6::TraceGraphicsBackend::VulkanDirect, 3, true}),
                    "second contiguous tick writes");
  failures += check(writer.event_count() == 12, "six events per tick");
  failures += check(writer.close(), "trace closes");

  std::ifstream input(path);
  std::vector<std::string> lines;
  for (std::string line; std::getline(input, line);) lines.push_back(line);
  failures += check(lines.size() == 12, "twelve JSONL records persisted");
  failures += check(lines[0].find("\"sequence\":0,\"tick\":1,\"domain\":\"input\"") !=
                        std::string::npos,
                    "input event is first");
  failures += check(lines[4].find("\"domain\":\"media\"") != std::string::npos,
                    "media event is fifth");
  failures += check(lines[6].find("\"sequence\":6,\"tick\":2,\"domain\":\"input\"") !=
                        std::string::npos,
                    "second tick sequence continues");
  failures += check(lines[2].find("\"id\":1") < lines[2].find("\"id\":2"),
                    "objective rows are stable by id");
  failures += check(lines[4].find("\"qualified\":false") != std::string::npos,
                    "unqualified media remains explicit");
  failures += check(lines[11].find("\"simulation\":\"") != std::string::npos,
                    "hashes name the simulation domain");
  std::remove(path);
  return failures == 0 ? 0 : 1;
}
