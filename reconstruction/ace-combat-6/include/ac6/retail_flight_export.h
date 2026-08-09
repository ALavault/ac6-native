#pragma once

// The flight model's two export functions, ported from 0x82303898 and
// 0x82303940 -- slots 20 and 21 of the vtable 0x8200F310.
//
// They are the boundary between the flight model and whatever consumes it: each
// takes a caller-supplied buffer in r4 and fills part of it with the model's
// control outputs and a handful of raw fields. The base vtable leaves both slots
// as the shared empty `blr`, so they exist only on the live branch.
//
// THEY ANSWER CYCLE 1391'S OPEN QUESTION. That cycle noted there was no blend
// accessor for the middle axis and left it there. Slot 16 (0x822B65C8) is TWO
// INSTRUCTIONS -- `lfs f1,308(r3); blr` -- so the middle axis is exported RAW
// while the outer two go through retail_control_blend. Two blended, one not.
//
//     slot 16   0x822B65C8   [+308]                          raw
//     slot 17   0x82281E10   blend of [+304], [+408], [+144]  retail_control_blend
//     slot 18   0x82281EA8   blend of [+312], [+412], [+152]  retail_control_blend
//
// SLOT 20 CALLS THEM IN THE ORDER 16, 17, 18 AND STORES THEM AT +12, +8, +16.
// The order of the calls and the order of the destinations disagree, which is
// visible only because both were read; a port that assumed they matched would
// swap two of the three outputs.
//
// AND +376 IS WRITTEN TWICE by slot 20, to +28 and to +52. Not a transcription
// slip -- 0x823038F8 and 0x82303920 both load it and both store it. Slot 21
// writes +52 as well, from the same source, so the two exporters overlap on that
// one word and agree about it.

#include <cstdint>

namespace ac6::retail {

// What the three accessors return. Named by slot because that is what the
// exporters call, and because slot 16's value is the raw axis while the other
// two have been through the blend.
struct FlightAccessorValues {
  float slot16{};   // [+308], raw
  float slot17{};   // blended
  float slot18{};   // blended
  bool operator==(const FlightAccessorValues&) const = default;
};

// The raw model fields the exporters copy without touching.
struct FlightExportFields {
  float at32{};
  float at356{};
  float at368{};
  float at372{};
  float at376{};
  bool operator==(const FlightExportFields&) const = default;
};

// The caller's buffer, by byte offset. Passed in and returned so that the
// offsets a given exporter does NOT write keep whatever the caller had -- which
// is what retail does, and what a memset-then-fill port would destroy.
struct FlightExportBlock {
  float at8{}, at12{}, at16{}, at20{}, at24{}, at28{}, at32{}, at36{};
  float at44{}, at48{}, at52{}, at56{};
  bool operator==(const FlightExportBlock&) const = default;
};

// Slot 20 writes +8, +12, +16, +20, +24, +28, +32, +36 and +52. Nine words.
FlightExportBlock export_flight_state_slot20(
    FlightExportBlock block, const FlightAccessorValues& values,
    const FlightExportFields& fields) noexcept;

// Slot 21 writes +44, +48, +52 and +56. Four words, and +52 is the overlap.
FlightExportBlock export_flight_state_slot21(
    FlightExportBlock block, const FlightAccessorValues& values,
    const FlightExportFields& fields) noexcept;

}  // namespace ac6::retail
