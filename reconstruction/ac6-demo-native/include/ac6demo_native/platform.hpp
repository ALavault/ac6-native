#pragma once

#include <cstdint>
#include <string>

namespace ac6demo_native {

struct XInputFrame {
    std::uint16_t buttons = 0U;
    std::uint8_t left_trigger = 0U;
    std::uint8_t right_trigger = 0U;
    std::int16_t left_x = 0;
    std::int16_t left_y = 0;
    std::int16_t right_x = 0;
    std::int16_t right_y = 0;
    bool connected = true;

    [[nodiscard]] static constexpr XInputFrame neutral() noexcept {
        return {};
    }
    bool operator==(const XInputFrame&) const = default;
};

struct PlatformObservation {
    std::uint64_t tick = 0U;
    std::uint64_t present_count = 0U;
    std::uint64_t simulation_time_ns = 0U;
    XInputFrame input{};

    bool operator==(const PlatformObservation&) const = default;
};

class PlatformRuntime {
public:
    static constexpr std::uint64_t simulation_hz = 60U;
    static constexpr std::uint64_t nominal_present_interval = 2U;
    static constexpr std::uint64_t max_tick = 1000000000U;

    [[nodiscard]] bool step(const XInputFrame& input, std::string* error = nullptr);
    [[nodiscard]] bool notify_present(std::string* error = nullptr);
    [[nodiscard]] PlatformObservation observe() const noexcept;
    void reset() noexcept;

private:
    std::uint64_t tick_ = 0U;
    std::uint64_t presents_ = 0U;
    std::uint64_t last_present_tick_ = 0U;
    XInputFrame input_{};
};

}  // namespace ac6demo_native
