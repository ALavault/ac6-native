#pragma once

// The input binding layer, ported from 0x82211C10.
//
// This is where a controller axis becomes a gameplay value. `retail_input_record`
// gives the record; this gives what the game does with it, and the two meet at
// one rule that was derived from opposite ends:
//
//     addi   r11,r10,3
//     rlwinm r11,r11,2,0,29        r11 = (bit + 3) * 4
//     lfsx   f13,r11,r4            read that slot of the record
//
// `float_slot_for_bit` in retail_input_record.h says exactly that. It was derived
// at cycle 1318 from the PRODUCER's bit-scan at 0x821CAED4, ported at 1323, and
// read here in the CONSUMER at 1353 -- a function sharing no code, no register
// allocation and no derivation with it.
//
// WHERE IT SITS. 0x82211DF8 walks the four 0xA0-byte records at 0x826EDB98 and
// calls 0x82211C10 once per player, handing record i to player block i at a
// stride of 912 bytes, with four output arrays. Both loop bounds -- base + 640,
// stride 160 -- agree with the base and stride cycle 1320 measured by executing
// the producer.
//
// THE TABLE IS THE POINT. Every deadzone, scale and threshold is per binding, out
// of a 24-byte descriptor, not a constant in the code. A port that hard-codes any
// of them is not this function.

#include <cstdint>

namespace ac6::retail {

// One 24-byte descriptor, at player + 24 * (record_bit + 6).
// The first word is NOT read by 0x82211C10 and is not modelled as anything.
struct InputBinding {
  float unread{};      // +0x00
  float processed{};   // +0x04, written by the layer
  float raw{};         // +0x08, written by the layer
  float threshold{};   // +0x0C
  float deadzone{};    // +0x10
  float scale{};       // +0x14
  bool operator==(const InputBinding&) const = default;
};

// The two floats one binding produces.
struct BindingOutputs {
  float value{};   // deadzone-subtracted, scaled, clamped, sign restored
  float step{};    // see below -- NOT simply a sign
  bool operator==(const BindingOutputs&) const = default;
};

// `fsel(a, b, c)` is `a >= 0 ? b : c`, and the comparison is against +0.0, so
// **negative zero takes the positive branch**. That is not a detail here: cycle
// 1323 measured that an idle axis leaves NEGATIVE zero in the record, so every
// idle binding goes through this function with a = -0.0 and must come out on the
// `b` side. A port using `value < 0.0f` agrees; one using `std::signbit` does not.
constexpr float select_ge_zero(float a, float b, float c) noexcept {
  return a >= 0.0F ? b : c;
}

// value:
//     f0 = |v| - deadzone
//     if f0 < 0   -> f0 = 0
//     else          f0 = f0 * scale ; if f0 > 1 -> f0 = 1
//     result = fsel(v, f0, -f0)
//
// step -- and CYCLE 1353 DESCRIBED THIS WRONG. It called it a "three-state sign",
// 0 or +-1. Reading the branch targets while porting shows a middle band:
//
//     f0 = v
//     if |v| <  deadzone   -> f0 = 0
//     if |v| >  threshold  -> f0 = fsel(f0, +1, -1)
//     otherwise f0 keeps the RAW VALUE
//
// so it is zero inside the deadzone, saturated beyond the threshold, and the
// untouched input in between. Three regions, and only two of them are constant.
BindingOutputs apply_input_binding(float value, const InputBinding& binding) noexcept;

// Both outputs are negated when the player's invert mask has the slot's bit.
BindingOutputs invert_outputs(BindingOutputs outputs) noexcept;

// The record slot a binding reads, and the descriptor index it lives at. Both are
// the retail arithmetic, kept here so a caller cannot get them subtly different.
constexpr unsigned binding_descriptor_index(unsigned record_bit) noexcept {
  return record_bit + 6;
}

}  // namespace ac6::retail
