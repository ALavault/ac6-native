# Cycle 170 — NDXR callback result and numeric helper `0x822c2868`

Date: 2026-07-18 (Europe/Paris)

## Target and provenance

Canonical AC6 Xbox 360 PAL target: `default.xex`, target ID
`ac6-xbox360-pal`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`, image
base `0x82000000`, Ghidra project `ace-combat-6`.

The reader body remains bounded by `.pdata` at
`0x82102148..0x82102567`. This pass reads only canonical XenonRecomp output
and does not modify generated output or the Ghidra project.

## Correction to the previous callback wording

The indirect callback is at `0x821023a0`; `0x821023a4` is the return address.
After `bctrl`, the callback's `r3` is copied to `r7` and passed to
`0x822c2868`. It is therefore a pointer or handle-like result consumed by the
next helper, not a boolean whose low byte directly controls the reader loop.

The boolean tested by the reader caller is the low byte of the result returned
by `0x822c2868` (and, at the later call-site, by `0x82102568`). The previous
cycle's statement that the callback return itself controlled continuation is
replaced by this more precise qualification.

## Call boundary at `0x82102410`

The reader calls `0x822c2868` with the following values after the callback:

```text
r3 = r1 + 0x70   # four-float scratch/context; helper writes its result here
r4 = r1 + 0x90   # vector output area for this helper
r5 = r14         # auxiliary pointer preserved from the reader's incoming r7
r6 = [r1 + 0x1cc]# auxiliary pointer preserved from incoming r8
r7 = callback r3 # descriptor/table pointer or handle; null returns 0
r8 = caller value retained at helper stack +556
r9 = low 16 bits of the current record word
r10 = current index/coordinate value (not retyped)
```

The exact semantic types of `r5`, `r6`, `r7`, `r9` and `r10` remain unknown.
The helper's prologue confirms that `r3`, `r4`, `r5`, `r6` and `r9` are retained
as long-lived values (`r19`, `r30`, `r29`, `r28`, `r27`).

## What `0x822c2868` actually does

The helper performs a bounded descriptor lookup and numeric/VMX processing:

1. A null `r7` returns `0` immediately.
2. It reads a 32-bit pointer/identifier at `r7+0`, compares the pointed bytes
   against embedded strings, and advances by `0x10` when the entry does not
   match.
3. A candidate entry must have byte `+8 == 2` and byte `+9 == 0`; it then reads
   words at `+16`, `+20`, `+24` and halfwords at `+10`, `+12`, `+14`.
4. It applies the same `0..15` shifted-index checks observed around the reader
   and computes table offsets from the selected record. The record has an
   additional `+38` count and `+40` data reference used by a small type/switch
   dispatch.
5. `r5` and `r6` are read at offsets `0`, `4` and `8` as three-float records.
   They are used as lower/upper numeric bounds while the helper builds VMX
   vectors. This confirms the record shape but not its gameplay meaning.
6. A nested call to `0x822c20c8` validates or transforms each generated
   element. Its low-byte result controls whether the candidate vector is
   committed.
7. On a successful path, the helper writes a 16-byte vector to `r4` and a
   scalar/flag at `r3+12`; it also writes a 16-byte vector to `r3+0`. The
   scalar is initialized to the float constant `0.0` at entry and the helper
   returns a byte-sized success value. On rejected/absent records it returns
   `0` without exposing a semantic result.

The code uses VMX128 vector loads, `vmsum4fp128`, floating-point comparisons,
and a bounded table dispatch. It must therefore preserve big-endian loads,
single-precision rounding, vector lane order and the exact `NaN`/comparison
behavior in any future native wrapper.

## Consequences for the reader contract

- The callback return is now `callback_result -> 0x822c2868.r7`, classified as
  `unknown pointer/handle`, not as a boolean.
- The first success/failure boolean after the callback is the return from
  `0x822c2868` at `0x82102410`.
- The helper can overwrite the vector/scalar scratch outputs. A successful
  callback alone is therefore insufficient evidence of a final reader result.
- The later `0x82102568` helper remains a separate post-reader path with its
  own ABI and can replace the same scalar/vector results.
- No C++ class, method name, unit, or gameplay meaning is promoted. The safe
  representation is an ABI-level differential harness with binary-qualified
  scratch areas and callback-result provenance.

## Confidence

- `confirmed`: callback address `0x821023a0` and post-call copy `r3 -> r7`.
- `confirmed`: null-result guard and descriptor-entry checks in
  `0x822c2868`.
- `confirmed`: three-float reads from the two auxiliary pointers and writes to
  the `r3`/`r4` output areas.
- `confirmed`: helper success byte, vector operations and nested call to
  `0x822c20c8`.
- `unknown`: callback signature, descriptor class, record units, table
  ownership and the meaning of the vector/scalar outputs.

## Validation and limits

No human/VNC/Xenia session is required for this ABI qualification. No native
source or generated recompilation output was changed. The next useful step is
to correlate `0x822c20c8` with its callers and to build a bounded differential
harness; do not assign a gameplay name before that evidence exists.

