#pragma once

#include "ac6/retail_session_replay.h"

#include <cstdint>
#include <span>

namespace ac6::retail::detail {

bool replay_v3_matches(std::span<const std::uint8_t> bytes,
                       const RetailSessionReplay &replay);

} // namespace ac6::retail::detail
