#include "ac6demo/trace.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main() {
  const auto path = std::filesystem::temp_directory_path() / "ac6-demo-trace-test.jsonl";
  std::error_code error;
  std::filesystem::remove(path, error);
  {
    ac6demo::TraceWriter writer;
    writer.open(path, ac6demo::GraphicsBackend::Headless);
    writer.append(0U, ac6demo::TraceDomain::Input,
                  "{\"buttons\":0,\"left_trigger\":1,\"right_trigger\":2,"
                  "\"lx\":-3,\"ly\":4,\"rx\":-5,\"ry\":6,"
                  "\"connected\":true}");
    writer.append(0U, ac6demo::TraceDomain::Input,
                  "{\"xam_state_polls\":1,\"controller_snapshot\":true,"
                  "\"current_buttons\":16,\"pressed_buttons\":16}");
    writer.append(0U, ac6demo::TraceDomain::Simulation, "{\"state\":\"frontend\"}");
    writer.close();
    assert(writer.count() == 3U);
  }
  const auto replay = ac6demo::TraceReader::read(path);
  replay.validate();
  assert(replay.header().version == 4U);
  assert(replay.events().size() == 3U);
  const auto inputs = replay.input_events();
  assert(inputs.size() == 1U);
  assert(inputs.front().tick == 0U);
  const ac6demo::InputFrame expected_input{0U, 1U, 2U, -3, 4, -5, 6, true};
  assert(inputs.front().frame == expected_input);

  const auto incomplete_path = path.string() + ".incomplete";
  {
    ac6demo::TraceWriter writer;
    writer.open(incomplete_path, ac6demo::GraphicsBackend::Headless);
    writer.append(0U, ac6demo::TraceDomain::Input, "{\"buttons\":0}");
    writer.close();
  }
  bool incomplete_rejected = false;
  try {
    (void)ac6demo::TraceReader::read(incomplete_path).input_events();
  } catch (const ac6demo::RuntimeTrap&) {
    incomplete_rejected = true;
  }
  assert(incomplete_rejected);

  const auto old_path = path.string() + ".old";
  {
    std::ofstream old(old_path);
    old << "{\"magic\":\"AC6RTPLY\",\"version\":3,\"xex_sha256\":\""
        << ac6demo::kQualifiedXexSha256
        << "\",\"backend\":\"headless\"}\n";
  }
  bool rejected = false;
  try {
    ac6demo::TraceReader::read(old_path).validate();
  } catch (const ac6demo::RuntimeTrap&) {
    rejected = true;
  }
  assert(rejected);
  std::filesystem::remove(path, error);
  std::filesystem::remove(old_path, error);
  std::filesystem::remove(incomplete_path, error);
  std::cout << "ac6-demo-trace-tests: ok\n";
  return 0;
}
