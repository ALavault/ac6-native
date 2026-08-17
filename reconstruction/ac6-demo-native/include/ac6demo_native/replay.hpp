#pragma once

#include "ac6demo_native/platform.hpp"

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ac6demo_native {

inline constexpr std::size_t max_replay_records = 1000000U;
inline constexpr std::size_t max_replay_bytes = 256U * 1024U * 1024U;

[[nodiscard]] std::string write_replay_journal(
    std::span<const PlatformObservation> observations, std::string* error = nullptr);

[[nodiscard]] std::optional<std::vector<PlatformObservation>> replay_journal(
    std::string_view journal, std::string* error = nullptr);

}  // namespace ac6demo_native
