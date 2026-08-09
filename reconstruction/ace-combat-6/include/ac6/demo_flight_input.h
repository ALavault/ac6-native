#pragma once

// A controller, wired to the contracted flight chain.
//
// WHAT CHANGED AT CYCLE 1407. This file used to invent the conversion from a
// binding output to a flight command -- a "full-scale angle" and an "increment
// rate" it made up, feeding the virtual setters. Cycles 1405 and 1406 found
// retail's actual player path and contracted it, and it has none of those
// things:
//
//     the entity's field  ->  accumulator += field, clamped
//
// straight through, except one axis which takes a difference. So the invented
// conversion is DELETED, not reduced. What remains invented is smaller and
// different in kind, and it is listed below rather than described.
//
// CONTRACTED, MEASURED, end to end:
//   build_input_record        0x821CAA50
//   apply_input_binding       0x82211C10
//   route_flight_input_fields 0x82229250  -- outputs to the six entity fields
//   apply_flight_input        0x82227E10  -- the five increments
//   accumulate_flight_input   five direct functions
//   ...and the twenty-two behaviours of the flight chain after them.
//
// WHAT CHANGED AT CYCLE 1409. The invention shrank again. Cycle 1409 read the
// whole of 0x82229250's routing block and found that ALL SIX entity fields come
// from six consecutive floats of the binding layer's first output array, routed
// by a device mode and a layout word. That routing is ported and contracted as
// `route_flight_input_fields`, so the step from a binding OUTPUT to an entity
// FIELD is no longer a choice.
//
// STILL INVENTED, and only this:
//   WHICH CONTROLLER AXIS FEEDS WHICH BINDING SLOT. The per-player table that
//   decides it is loaded from data this campaign has not reached. A wrong choice
//   here permutes the six slots; it cannot change what happens to them
//   afterwards, because that part is retail's.
//   Also chosen: that the demo drives the analog arm at layout 0.
//
// The difference from the old invention matters. Before, the ARITHMETIC was
// mine, so the aeroplane could respond to a stick in a way retail never would.
// Now only the WIRING is mine: every number that reaches the flight model has
// been through retail's own rules, and a wrong choice here swaps two axes rather
// than changing how the aircraft flies.

#include "ac6/retail_flight_input_apply.h"
#include "ac6/retail_flight_input_router.h"
#include "ac6/retail_input_binding.h"
#include "ac6/retail_input_record.h"

#include <cstdint>

namespace ac6::demo {

// The bindings a stick needs. The descriptor type is retail's; its CONTENTS are
// chosen, because the per-player table that fills them is loaded from data this
// campaign has not reached.
struct StickBindings {
  ac6::retail::InputBinding pitch{};
  ac6::retail::InputBinding roll{};
  ac6::retail::InputBinding yaw{};
  ac6::retail::InputBinding throttle{};
  bool operator==(const StickBindings&) const = default;
};

// A deadzone of about 8%, unit scale, a threshold at half. Chosen, not read.
StickBindings default_stick_bindings() noexcept;

// Contracted path, chosen wiring: snapshot -> record -> bindings -> the six
// entity fields 0x82227E10 reads.
ac6::retail::FlightInputFields fields_from_record(
    const ac6::retail::InputRecord& record,
    const StickBindings& bindings) noexcept;

ac6::retail::FlightInputFields fields_from_snapshot(
    const std::uint8_t* snapshot, const StickBindings& bindings) noexcept;

}  // namespace ac6::demo
