#pragma once

// The contracted flight chain, wired together and stepped frame by frame.
//
// WHAT THIS IS. Every other header in this thread ports one retail function and
// is verified against it. This one ports NO retail function: it composes the
// contracted ones in the order the retail step 0x82306A38 calls them, so that a
// caller can hand it a stick position per frame and read back the aeroplane's
// attitude. It is the substrate a demo draws, and the first thing in A3.2 that
// is a *system* rather than a function.
//
// BECAUSE IT PORTS NOTHING, IT CANNOT HAVE A DIFFERENTIAL, and it does not enter
// the contract. Its correctness is entirely the correctness of the pieces --
// each of which has one -- plus the ORDER, which is taken from the contracted
// retail_live_flight_step and is the only thing this file asserts on its own.
//
// THE ORDER, from 0x82306A38 and its callees:
//
//   1. the three command setters (slots 12/13/14)  retail_flight_command
//   2. slot 30: the ramps, then the axes           retail_live_flight_ramps
//                                                   retail_live_flight_axes
//   3. the rate servo (a direct call)              retail_flight_rate_servo
//   4. the decays                                  retail_live_flight_step
//   5. the rotation angles                         retail_flight_orientation
//      applied to the basis in A3.1's order        retail_transform
//   6. the export block (slots 20/21)              retail_flight_export
//
// WHAT IS MISSING, AND IT IS NOT SMALL. **Position is not integrated here.**
// The live model's position step is slot 31, 0x823042D0, 505 instructions of
// vector aerodynamics, and cycle 1383 established it is unreachable by this
// campaign's instrument for a HARDWARE reason: its normalise depends on vrefp
// and vrsqrtefp, estimate instructions whose exact bits are a property of the
// console and which are therefore refused rather than modelled.
//
// So this produces ATTITUDE and CONTROL STATE, not a trajectory. The aeroplane
// pitches, rolls and yaws in response to the stick; it does not move. Saying so
// here is the point -- a demo built on this must say the same.

#include "ac6/retail_control_blend.h"
#include "ac6/retail_flight_command.h"
#include "ac6/retail_flight_input_apply.h"
#include "ac6/retail_flight_export.h"
#include "ac6/retail_flight_orientation.h"
#include "ac6/retail_flight_rate_servo.h"
#include "ac6/retail_flight_step.h"
#include "ac6/retail_live_flight_axes.h"
#include "ac6/retail_live_flight_ramps.h"
#include "ac6/retail_live_flight_step.h"
#include "ac6/retail_transform.h"

#include <cstdint>

namespace ac6::retail {

// The stick, per frame. Each `target` is an angle in radians and each
// `increment` is what the command accumulator gains when the target is accepted
// -- retail's own two arguments to slots 12, 13 and 14, not an invention.
struct FlightStick {
  float target12{};
  float increment12{};
  float target13{};
  float increment13{};
  float target14{};
  float increment14{};
  bool operator==(const FlightStick&) const = default;
};

// The per-aircraft numbers the chain reads. Named by the offset each comes from
// so the mapping stays checkable against the derivations.
struct FlightModelConfig {
  FlightRotationLimits limits{};        // +1248, +1252, +1256
  LiveAxisRates rates304{};             // [+576+0], [+592+0]
  LiveAxisRates rates308{};             // [+576+4], [+592+4]
  LiveAxisRates rates312{};             // [+576+8], [+592+8]
  RateServoAxis servo304{};             // gains from +544 / +560
  RateServoAxis servo308{};             // +548 / +564
  RateServoAxis servo312{};             // +552 / +568
  float rampRate952{};                  // +952
  float rampRate956{};                  // +956
  float rampThreshold404{};             // +404
  float row0Divisor{kRow0Divisor};      // 7.0 unless [+332] bit 4
  bool rampGate1224{true};              // +1224
  bool operator==(const FlightModelConfig&) const = default;
};

// Everything the chain carries between frames.
struct FlightSessionState {
  float current16{};      // the three angles the setters compare against
  float current20{};
  float current24{};
  // The five accumulators. Two interfaces write them: the virtual setters
  // (retail_flight_command, the AI's) and the five direct accumulators
  // (retail_flight_input_accumulators, the player's). Both are contracted, and
  // the session offers a step for each.
  FlightInputAccumulators accumulators{};
  LiveRampState ramps{};
  LiveAxisState axes{};
  FlightRates3 rates{};
  LiveStepDecays decays{};
  RetailBasis basis{identity_basis()};
  // The integrated position, at [model+112]+96+64/68/72 in retail. `at68` is the
  // vertical one -- it is the only component carrying the 10.0 floor and the only
  // one the gravity bias is subtracted from (retail_flight_step.h).
  FlightPosition position{};
  std::uint32_t flags332{};
  bool operator==(const FlightSessionState&) const = default;
};

// What one frame produced, for a caller that wants to draw or log it.
struct FlightFrame {
  FlightAccessorValues accessors{};
  FlightExportBlock exported{};
  FlightRotationAngles angles{};
  bool accepted12{};
  bool accepted13{};
  bool accepted14{};
  bool operator==(const FlightFrame&) const = default;
};

// Steps the chain once. The basis is rotated in A3.1's order -- row 1, then
// row 0, then row 2 -- which cycle 1376 showed slot 32 uses and which A3.1
// derived from a different caller entirely.
// The AI's interface: three target angles with increments, through the virtual
// setters. Anything within a degree of the model's current angle is discarded.
FlightFrame step_flight_session(FlightSessionState& state,
                                const FlightModelConfig& config,
                                const FlightStick& stick, float step) noexcept;

// The PLAYER's interface, and the one a controller actually drives: six entity
// fields, through 0x82227E10's five loads and one subtraction into the five
// clamped accumulators. Cycle 1405 established these are different interfaces
// onto the same fields, not two names for one.
FlightFrame step_flight_session(FlightSessionState& state,
                                const FlightModelConfig& config,
                                const FlightInputFields& fields,
                                float step) noexcept;

// ------------------------------------------------------------------ position
//
// SEPARATE FROM THE STEP ABOVE, AND ON PURPOSE. Everything `step_flight_session`
// composes is contracted retail arithmetic driven by contracted retail inputs.
// This is not, and the difference is the whole reason it is a second call with
// its own arguments rather than a line inside the first.
//
// `integrate_flight_position` (0x82303110) IS contracted. What is not available
// is what feeds it. Cycle 1415 read the window: the three rates are loaded from
// stack slots 80/84/88 at 0x82303524..0x8230352C, and those slots hold the
// output of a VECTOR NORMALISE at 0x823034AC..0x82303520 whose seeds are
// `vrsqrtefp` (0x823034CC) and `vrefp` (0x823034FC) -- both estimate
// instructions, specified only to a relative accuracy bound, and refused by this
// campaign rather than approximated.
//
// So retail's rates are a DIRECTION, normalised to unit length behind that
// boundary, and `rate_scale` -- [model+32], clamped against [model+1264] above
// and 0.0 below at 0x82303530..0x82303554 -- is the SPEED that scales it.
//
// A caller therefore has to supply the direction and the speed itself. Doing so
// is an invention and the caller must say so; what it gets in exchange is that
// the integration, the floor, the gravity bias and the fusing are retail's.
//
// `mid_bias` is retail's f25*f24. f25 is 9.8/3.6 scaled by [model+344]*30 when
// that field is non-zero; f24 is unresolved (retail_flight_step.h). Passing 0.0
// integrates without gravity, which is a choice and not a reading.
void integrate_session_position(FlightSessionState& state,
                                FlightRates rates,
                                float rate_scale,
                                float mid_bias,
                                float step) noexcept;

// A 64-bit FNV-1a over the whole state, so a replay can be pinned to one number.
// The same digest the input log uses, for the same reason: a trajectory that
// changes is a regression, and a column of floats is not a thing to diff by eye.
std::uint64_t digest_flight_state(const FlightSessionState& state) noexcept;

}  // namespace ac6::retail
