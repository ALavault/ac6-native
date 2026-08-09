#pragma once

// How the entity's per-frame input fields reach the flight model, ported from
// the tail of 0x82227E10 (0x82227E98..0x82227EDC).
//
// THIS IS THE LAST LINK. The chain from a controller to the aeroplane is now
// contracted end to end:
//
//   build_input_record        0x821CAA50   retail_input_record
//   apply_input_binding       0x82211C10   retail_input_binding
//   route_flight_input_fields 0x82229250   retail_flight_input_router
//   THIS                      0x82227E10   the five increments
//   accumulate_flight_input   five funcs   retail_flight_input_accumulators
//   the whole flight chain                 twenty-five behaviours
//
// AND IT IS FIVE LOADS AND ONE SUBTRACTION:
//
//     [+2096]            -> 0x82281F40 -> accumulator +48   a hold
//     [+2100]            -> 0x82281F78 -> accumulator +52   a hold
//     [+2104]            -> 0x82281FB0 -> accumulator +36   an axis
//     [+2112] - [+2116]  -> 0x82281FE8 -> accumulator +44   an axis
//     [+2108]            -> 0x82282020 -> accumulator +40   an axis
//
// Each field is passed STRAIGHT THROUGH as the increment. There is no scaling,
// no rate, no full-scale angle -- the demo's invented conversion supposed all
// three and retail has none of them. The one piece of arithmetic in the whole
// step is the DIFFERENCE for +44, and it is the detail that a port written from
// the shape would miss: four pass-throughs and one subtraction do not look like
// a rule, so they get "tidied" into five pass-throughs.
//
// THE ORDER MATTERS AND IS PRESERVED. Retail calls the accumulators in the order
// 48, 52, 36, 44, 40 -- not in field order and not in axis order. Each
// accumulator reads the field it is about to write, so two calls to the same one
// would compose; they are five distinct fields here, so the order is not
// observable through them. It is preserved anyway, because "not observable
// today" is not "safe to reorder".
//
// WHERE THE FIELDS COME FROM, and this is settled. Cycle 1404 established that
// 0x82229250 copies the contracted binding layer's first two output values --
// [player+0xE58] and [player+0xE5C] -- into +2104 and +2108, and said of the
// other four that it did not know. Cycle 1409 read the rest of the same
// function: they are the SAME ARRAY, four slots further along, at
// [player+0xE60 .. 0xE6C]. All six entity fields are the binding layer's first
// six outputs, routed by a device mode and a layout word.
//
// `retail_flight_input_router.h` carries the table and the port. Note that on
// two of the three arms some of these fields hold 1.0 or 0.0 from a button bit
// rather than an analog value -- so a caller reading this header alone would be
// wrong to assume all six are continuous.

#include "ac6/retail_flight_input_accumulators.h"

#include <cstdint>

namespace ac6::retail {

// The six entity fields the step reads, by byte offset.
struct FlightInputFields {
  float at2096{};
  float at2100{};
  float at2104{};
  float at2108{};
  float at2112{};
  float at2116{};
  bool operator==(const FlightInputFields&) const = default;
};

// The five accumulators of the flight model, in field order.
struct FlightInputAccumulators {
  float at36{};
  float at40{};
  float at44{};
  float at48{};
  float at52{};
  bool operator==(const FlightInputAccumulators&) const = default;
};

// Applies one frame of input. Returns the accumulators after all five calls, in
// retail's order: 48, 52, 36, 44, 40.
FlightInputAccumulators apply_flight_input(FlightInputAccumulators state,
                                           const FlightInputFields& fields) noexcept;

}  // namespace ac6::retail
