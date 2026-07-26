# AC6 DATA payload decoder recovery

Date: 2026-07-15

## Evidence chain

- PowerPC references to `sim:DATA.TBL`, `game:\DATA00.PAC` and
  `game:\DATA01.PAC` lead to `0x821cc250`, `0x821cc338` and the loader state
  machine at `0x821cc4d0`.
- The RTTI descriptor `.?AVCAce6Uncompress@ACE6@@` is at `0x826e8d3c`.
  Loader requests construct that class with `0x822cf618` and initialize it via
  `0x822cf658`.
- `0x822cf920` dispatches compression modes 0, 1 and 2 to the custom LZ leaf
  `0x822cd348`, the DEFLATE state machine at `0x822cf708`, and the copy leaf at
  `0x822cd238` respectively.
- `0x822cf428` generates an eight-byte XOR key. Its helpers implement Machin's
  formula `pi = 4 * (4 * atan(1/5) - atan(1/239))`; the selected key uses two
  consecutive 32-bit base-2^32 digits and cycles every 256 catalog indices.
- Mode 1 is raw DEFLATE: the recovered code implements stored, fixed-Huffman
  and dynamic-Huffman blocks, including the standard LEN/NLEN check.

## Native parity result

`reconstruction/ace-combat-6/` now contains a clean-room C++20 implementation
of key generation, XOR, modes 0/1/2, bounded PAC extraction and a full archive
validator. The supplied retail set passed all 926 payloads:

- decoded bytes: 5,424,368,676;
- payloads with valid leading `FHM ` signature: 926/926;
- bounded top-level FHM directories: 926/926, exposing 4,820 member slots,
  including 2,811 non-empty members;
- entry 0: 3,389,860 -> 9,120,864 bytes, key
  `85a308d313198a2e`;
- entry 39: 928 -> 928 bytes, key `fb21a991487cac60`.

This establishes payload decoding and the first inner-container directory
boundary. It does not yet recursively classify or name the FHM members.
