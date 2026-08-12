#pragma once

#include "ac6/retail_session.h"

#include <cstddef>
#include <cstdint>
#include <vector>

using RetailSessionInputFn = ac6::InputFrame (*)(std::size_t) noexcept;
using RetailSessionFrameHashFn =
    std::uint64_t (*)(const ac6::retail::RetailSessionFrame&);

void check_store_backed_session(const std::vector<std::uint8_t>& payload,
                                RetailSessionInputFn input,
                                RetailSessionFrameHashFn frame_hash);
