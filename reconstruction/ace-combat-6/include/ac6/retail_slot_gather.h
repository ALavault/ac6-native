#pragma once

// The active-slot gatherer, ported from 0x82211B40. The last piece of the frame
// input tick 0x821CA908, and the one that joins the contracted record to the
// contracted binding layer.
//
// It answers one question: which of a player's 32 output slots have any binding
// whose record bit is set this frame?
//
//   if [record + 0x08] == 0 -> return, having ORed nothing at all
//   for k in 0..31:
//       if [player + 0x08 + 4*k] & [record + 0x08] : result |= 1 << k
//
// `[record + 0x08]` is the record's flag word, which `retail_input_record`
// already carries and whose fourteen button bits `kHeldBitToRecordBit` maps. The
// 32 mask words at `player + 0x08` are the same ones `retail_input_binding`
// walks. This function is the only thing between them.
//
// WHERE IT SITS. 0x82211DF8 calls it once per object, before its four calls to
// 0x82211C10 and its final call to 0x82211988, so the mask this builds is the
// one the binding layer extends and the repeat bank turns into edges.
//
// IT CONTAINS NO FLOATING-POINT INSTRUCTION. Cycles 1356 and 1357 both named it
// as the consumer of the frame's elapsed time; it is not a consumer of anything
// float. The float passes through it untouched on its way to 0x82211988.
//
// `.pdata` has NO ROW for 0x82211B40, so its 51 instructions carry no length
// control and this port rests on the recompiled corpus alone -- the same
// situation as retail_slot_repeat, and the same reason its differential is worth
// more than usual.
//
// THE EARLY RETURN IS NOT THE SAME AS A ZERO RESULT IN RETAIL. The function ORs
// into a caller's word; on the early return it ORs nothing, which is what
// returning zero models here, but a port that assigned instead of ORing would
// clear bits the caller had already set.

#include <array>
#include <cstdint>

namespace ac6::retail {

inline constexpr std::size_t kSlotMaskCount = 32;

// The bits to OR into the caller's active-slot word. Retail ORs; so must a
// caller of this.
std::uint32_t gather_active_slots(
    std::uint32_t record_flags,
    const std::array<std::uint32_t, kSlotMaskCount>& slot_masks) noexcept;

}  // namespace ac6::retail
