#pragma once

// Where the entity's six input fields come from, ported from the routing block
// of 0x82229250 -- 0x82229310..0x8222946C.
//
// THIS CLOSES THE CHAIN AT THE OTHER END. `retail_flight_input_apply` names the
// six fields at entity+2096..+2116 and says of four of them that what fills them
// "is NOT established". It is established now, and the answer is that all six
// are the binding layer's first six outputs:
//
//     [binding+3672]  array[0]
//     [binding+3676]  array[1]
//     [binding+3680]  array[2]
//     [binding+3684]  array[3]
//     [binding+3688]  array[4]
//     [binding+3692]  array[5]
//
// Cycle 1404 had already found array[0] and array[1] going to +2104 and +2108.
// The other four are the same array, four slots further along -- which is the
// whole finding, because it means the wiring from a controller axis to a flight
// field is DERIVED and not chosen. The binding layer that fills that array is
// `retail_input_binding`, contracted at 0x82211C10.
//
// THE ROUTING IS NOT FIXED. Two words select among three arms:
//
//   * the DEVICE MODE, read before this block through a config chain
//     ([0x826F4EB4] + index*42976 + 19588, index clamped to 0..2). Zero selects
//     the digital arm at 0x82229310; anything else the analog arm at 0x82229370.
//   * the LAYOUT word [0x826EDB5C], compared against 1 at 0x82229374.
//
// | arm | +2096 | +2100 | +2104 | +2108 | +2112 | +2116 |
// |---|---|---|---|---|---|---|
// | digital device | array[4] | array[5] | array[0] | array[1] | bit 2 | bit 3 |
// | analog, layout != 1 | bit 4 | bit 5 | array[0] | array[1] | array[2] | array[3] |
// | analog, layout == 1 | array[3] | array[2] | array[0] | array[1] | bit 5 | bit 4 |
//
// The three arms begin at 0x82229310 (digital), 0x8222937C (analog, layout 0)
// and 0x822293E0 (analog, layout 1), and all three join a common tail at
// 0x82229460 that writes +2104 and +2108 and nothing else.
//
// LAYOUT 1 TRANSPOSES BOTH PAIRS AND REVERSES EACH. The analog pair that layout
// 0 sends to +2112/+2116 lands on +2096/+2100 with the two swapped, and the bit
// pair that layout 0 sends to +2096/+2100 lands on +2112/+2116, also swapped.
// That is the retail option that puts the throttle on the stick and the roll on
// the triggers, or the reverse; which of the two is which is a label this port
// does not have and does not invent.
//
// The first draft of this table had the layout-1 bits as 2 and 3, copied from
// the digital arm. `tools/audit_flight_input_router_microexec.py` rejected three
// values for it before a line of this file was written -- the bits are 5 and 4,
// read off `rlwinm r11,r11,0,26,26` at 0x822293F4 and `,27,27` at 0x82229420.
//
// BIT NUMBERING. `rlwinm rD,rS,0,MB,MB` keeps the single bit MB counted from the
// most significant, so it is the value 1 << (31 - MB). MB=29 is bit 2, MB=28 is
// bit 3, MB=27 is bit 4, MB=26 is bit 5. A set bit yields 1.0 (0x82001348) and a
// clear one 0.0 (0x8200082C); there is no intermediate.
//
// WHAT THIS DOES NOT PORT, and it is deliberate:
//
//   * the CONFIG CHAIN that produces the device mode. It is three indirections
//     through globals this port has no state for, so the mode is a parameter.
//   * the +2096 SUPPRESSION at 0x8222945C, which zeroes +2096 when the predicate
//     0x82228480 returns non-zero AND the entity byte at +10116 is zero. The
//     predicate is 35 unread instructions; the differential stubs it and
//     measures the path where it does not fire.
//   * the LAYOUT-1 RESPONSE CURVE at 0x82229470, which rewrites +2104 and +2108
//     as sign(x) * (1 - cos(|x| * pi/2)), snapped to sign(x) once that exceeds
//     0.9 (constants pi/2 at 0x82069E48 and 0.9 at 0x820078C0). It runs only on
//     layout 1 and it is a behaviour of its own; the fields this function
//     returns are the values BEFORE it. On layout 0 it does not run at all, so
//     for that layout these six values are final.
//
// Verified against the executed instructions by
// `tools/audit_flight_input_router_microexec.py`: 13 cases, 78 values, no
// tolerance.

#include "ac6/retail_flight_input_apply.h"

#include <cstdint>

namespace ac6::retail {

// The binding layer's first output array, [binding+3672 .. binding+3692].
struct FlightBindingOutputs {
  float value[6]{};
  bool operator==(const FlightBindingOutputs&) const = default;
};

// The device mode the config chain resolves to. Retail compares against zero
// and this port carries that comparison, not a name for what the modes are.
enum class FlightInputDevice : std::uint32_t {
  kDigital = 0,  // the arm at 0x82229310
  kAnalog = 1,   // the arm at 0x82229370; retail takes it for any non-zero mode
};

// Bit positions in [binding+3652], counted from the least significant.
inline constexpr std::uint32_t kDeviceAxisLowBit = 2;    // MB=29
inline constexpr std::uint32_t kDeviceAxisHighBit = 3;   // MB=28
inline constexpr std::uint32_t kAnalogHoldLowBit = 4;    // MB=27
inline constexpr std::uint32_t kAnalogHoldHighBit = 5;   // MB=26

// The layout value that transposes both pairs.
inline constexpr std::uint32_t kSwappedLayout = 1;

// Fills the six entity fields from the binding layer's outputs and button bits.
FlightInputFields route_flight_input_fields(const FlightBindingOutputs& outputs,
                                            std::uint32_t button_flags,
                                            FlightInputDevice device,
                                            std::uint32_t layout) noexcept;

}  // namespace ac6::retail
