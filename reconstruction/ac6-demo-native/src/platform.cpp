#include "ac6demo_native/platform.hpp"

namespace ac6demo_native {
namespace {

bool fail(std::string* error, const char* message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

std::uint64_t tick_time_ns(std::uint64_t tick) noexcept {
    constexpr std::uint64_t nanoseconds_per_second = 1000000000U;
    return (tick / PlatformRuntime::simulation_hz) * nanoseconds_per_second +
           ((tick % PlatformRuntime::simulation_hz) * nanoseconds_per_second) /
               PlatformRuntime::simulation_hz;
}

}  // namespace

bool PlatformRuntime::step(const XInputFrame& input, std::string* error) {
    if (tick_ == max_tick) {
        return fail(error, "platform tick budget exhausted");
    }
    input_ = input;
    ++tick_;
    return true;
}

bool PlatformRuntime::notify_present(std::string* error) {
    if (tick_ == 0U) {
        return fail(error, "PRESENT cannot precede the first tick");
    }
    if (last_present_tick_ == tick_) {
        return fail(error, "duplicate PRESENT in one tick");
    }
    last_present_tick_ = tick_;
    ++presents_;
    return true;
}

PlatformObservation PlatformRuntime::observe() const noexcept {
    return {tick_, presents_, tick_time_ns(tick_), input_};
}

void PlatformRuntime::reset() noexcept {
    tick_ = 0U;
    presents_ = 0U;
    last_present_tick_ = 0U;
    input_ = XInputFrame::neutral();
}

}  // namespace ac6demo_native
