#pragma once

// A controller, wired to the contracted flight chain.
//
// TWO HALVES, AND ONLY ONE OF THEM IS RETAIL'S.
//
// **Contracted, measured**: a controller snapshot becomes an `InputRecord` by
// `build_input_record` (0x821CAA50, contracted as `retail_input_record`), and
// each axis becomes a value and a step by `apply_input_binding` (0x82211C10,
// contracted as `retail_input_binding`). Both were verified against the retail
// instructions by micro-execution, including the details that decide branches:
// an idle axis leaves NEGATIVE ZERO in the record, and a binding whose processed
// value is exactly zero writes nothing at all.
//
// **Invented, and this file is named `demo_` for it**: the map from a binding's
// output to the flight model's command setters. Cycle 1393 established that
// slots 12, 13 and 14 take a TARGET ANGLE and an INCREMENT, and that they are
// the only writers of the model's commands. What it did NOT establish is who
// calls them, or with what -- that search was never run. So the conversion below
// is mine:
//
//     target    = axis value * a chosen full-scale angle
//     increment = |axis value| * a chosen rate
//
// If retail feeds those setters differently -- and it may well, since the
// setters compare a target against the model's CURRENT angle and discard
// anything within a degree -- then the stick will feel different from the game
// while every rule downstream of it stays exact. That is a sharp and honest
// division, and it is the last invented link in the chain.

#include "ac6/retail_flight_session.h"
#include "ac6/retail_input_binding.h"
#include "ac6/retail_input_record.h"

#include <cstdint>

namespace ac6::demo {

// The four bindings a stick needs, and the two chosen numbers. The bindings are
// retail's own 24-byte descriptor type; their CONTENTS here are chosen, because
// the per-player table that fills them is loaded from data this campaign has not
// reached.
struct StickBindings {
  ac6::retail::InputBinding pitch{};
  ac6::retail::InputBinding roll{};
  ac6::retail::InputBinding yaw{};
  float invented_full_scale_angle{1.2F};   // radians at full deflection
  float invented_increment_rate{1.0F};
};

// A reasonable set: a deadzone of about 8%, unit scale, a threshold at half.
// Chosen, not read -- retail's live values come from the binding table.
StickBindings default_stick_bindings() noexcept;

// The contracted path: snapshot -> record -> bindings -> a stick for the
// contracted flight session. `snapshot` is the 0x40 bytes retail copies, so
// snapshot[n] is device[n+4] -- the convention retail_input_record.h fixes.
ac6::retail::FlightStick stick_from_snapshot(const std::uint8_t* snapshot,
                                             const StickBindings& bindings) noexcept;

// The same, from an already-built record, for a caller that has one.
ac6::retail::FlightStick stick_from_record(
    const ac6::retail::InputRecord& record,
    const StickBindings& bindings) noexcept;

}  // namespace ac6::demo
