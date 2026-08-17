#include "ac6demo_native/replay.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::abort();
    }
}

std::vector<ac6demo_native::PlatformObservation> observations() {
    ac6demo_native::PlatformRuntime runtime;
    std::vector<ac6demo_native::PlatformObservation> result;
    for (std::uint64_t tick = 1U; tick <= 600U; ++tick) {
        auto input = ac6demo_native::XInputFrame::neutral();
        input.buttons = tick == 252U ? 16U : 0U;
        input.left_y = tick > 300U ? static_cast<std::int16_t>(-1234) : 0;
        require(runtime.step(input), "record action accepted");
        if (tick >= 254U && tick % 2U == 0U) {
            require(runtime.notify_present(), "record PRESENT accepted");
        }
        result.push_back(runtime.observe());
    }
    return result;
}

void test_round_trip() {
    const auto recorded = observations();
    std::string error;
    const std::string journal = ac6demo_native::write_replay_journal(recorded, &error);
    require(!journal.empty(), "journal written");
    const auto replayed = ac6demo_native::replay_journal(journal, &error);
    require(replayed.has_value() && *replayed == recorded, "journal replay is exact");
    require(ac6demo_native::write_replay_journal(*replayed, &error) == journal,
            "record and replay bytes are identical");
}

void test_fail_closed() {
    std::string error;
    std::string journal = ac6demo_native::write_replay_journal(observations(), &error);
    journal[journal.find("\"buttons\":16")] = 'X';
    require(!ac6demo_native::replay_journal(journal, &error).has_value(),
            "altered action rejected at first malformed record");

    journal = ac6demo_native::write_replay_journal(observations(), &error);
    journal.replace(journal.find("de917873"), 8U, "acc302c1");
    require(!ac6demo_native::replay_journal(journal, &error).has_value(),
            "retail identity rejected before replay");

    journal = ac6demo_native::write_replay_journal(observations(), &error);
    journal.erase(journal.rfind("{\"type\":\"hashes\""));
    require(!ac6demo_native::replay_journal(journal, &error).has_value(),
            "missing integrity trailer rejected");
}

}  // namespace

int main() {
    test_round_trip();
    test_fail_closed();
    std::cout << "replay tests passed\n";
    return 0;
}
