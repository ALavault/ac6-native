#include "ac6demo_native/platform.hpp"

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

std::vector<ac6demo_native::PlatformObservation> run_reference() {
    ac6demo_native::PlatformRuntime runtime;
    std::vector<ac6demo_native::PlatformObservation> result;
    result.reserve(600U);
    for (std::uint64_t tick = 1U; tick <= 600U; ++tick) {
        auto input = ac6demo_native::XInputFrame::neutral();
        input.buttons = tick == 252U ? 16U : 0U;
        require(runtime.step(input), "reference tick accepted");
        if (tick >= 254U && tick % 2U == 0U) {
            require(runtime.notify_present(), "guest PRESENT accepted");
        }
        result.push_back(runtime.observe());
    }
    return result;
}

void test_determinism_and_cadence() {
    const auto first = run_reference();
    const auto second = run_reference();
    require(first == second, "two platform runs are byte-value identical");
    require(first[251U].input.buttons == 16U, "typed button frame is poll-exact");
    require(first.back().tick == 600U, "one action advances one tick");
    require(first.back().present_count == 174U, "PRESENT is guest-notified only");
    require(first[59U].simulation_time_ns == 1000000000U,
            "60 ticks equal one deterministic second");
}

void test_fail_closed_boundaries() {
    ac6demo_native::PlatformRuntime runtime;
    std::string error;
    require(!runtime.notify_present(&error) && !error.empty(),
            "PRESENT before tick rejected");
    auto input = ac6demo_native::XInputFrame::neutral();
    input.connected = false;
    input.left_x = -32768;
    input.right_y = 32767;
    require(runtime.step(input, &error), "bounded signed extrema accepted");
    require(runtime.notify_present(&error), "first PRESENT accepted");
    require(!runtime.notify_present(&error), "duplicate PRESENT rejected");
    require(runtime.observe().input == input, "no HID fallback changes disconnected input");
    runtime.reset();
    require(runtime.observe() == ac6demo_native::PlatformObservation{},
            "reset returns deterministic neutral state");
}

}  // namespace

int main() {
    test_determinism_and_cadence();
    test_fail_closed_boundaries();
    std::cout << "platform tests passed\n";
    return 0;
}
