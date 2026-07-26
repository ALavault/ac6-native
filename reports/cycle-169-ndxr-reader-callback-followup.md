# Cycle 169 — NDXR reader callback and follow-up input records

Date: 2026-07-18 (Europe/Paris)

## Target and boundary

Canonical AC6 Xbox 360 PAL target: `default.xex`, target ID `ac6-xbox360-pal`,
SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
Ghidra project `ace-combat-6`, image base `0x82000000`.

The reader body is `0x82102148..0x82102567`; the following helper begins at
`0x82102568`.  All evidence is read-only and comes from canonical raw PPC,
`.pdata` and XenonRecomp output.

## Correction of the callback address

The indirect callback is at `0x821023a0`; `0x821023a4` is its return address
and the first post-call instruction.  The sequence is:

```text
0x82102360  lwz    r8,0x0(r31)       # receiver vtable/address-point
0x82102370  or     r3,r31,r31         # receiver
0x82102374  lwz    r8,0x5c(r8)        # virtual slot +0x5c
0x82102378  or     r4,r29,r29         # 9-bit record/index field
0x8210239c  mtspr  CTR,r8
0x821023a0  bctrl
0x821023a4  addi   r11,r1,0x80       # post-call scratch processing
```

The receiver's actual address-point is loaded at runtime.  The canonical
`0x8205c9a4` view has `+0x5c -> 0x82100600`, but this body alone does not prove
that every dynamic instance uses that view.

The callback return is copied to `r7` and consumed by the next helper
`0x822c2868`; its exact C++ signature remains unknown.  The boolean tested by
the reader caller is produced by that helper (and later by `0x82102568`), not
directly by the callback's low byte.  The preceding arithmetic does, however,
establish that `r5` and `r6` are already transformed numeric values at the
callback boundary; they must not be confused with the reader's incoming output
pointers, which were saved in the prologue.  See cycle 170 for the corrected
callback-result contract.

## Follow-up helper `0x82102568`

After each reader attempt, the caller can invoke `0x82102568` with:

```text
r3 = r1 + 0x70   # helper context/scratch
r4 = r1 + 0x80   # vector A produced by reader
r5 = r1 + 0x90   # vector B produced by reader
r6 = r1 + 0x60   # scalar result slot
r7 = r30         # auxiliary record pointer
r8 = r29         # auxiliary record pointer
r9 = r31         # coordinate/index
r10 = r27        # coordinate/index
```

At entry, the helper retains these as `r24`, `r30`, `r31`, `r19` and then
initializes `*(r6)` to `-1`.  It reads `r7` and `r8` at offsets `0`, `4` and
`8` as three-word floating-point records.  This confirms that the two
auxiliary values passed by the call-sites are pointers to 3-float records for
this follow-up path; their semantic type remains unknown.

The helper independently repeats the `r9/r10 >> 4` bounds checks (`0..15`) and
uses `r4/r5` as vector inputs.  It can therefore replace the scalar result and
vector outputs rather than merely observe them.  It is not safe to model the
reader and helper as one function or to assume that a successful reader result
is final.

## Qualification

- `confirmed`: callback address `0x821023a0`, receiver-vtable load and slot
  displacement `+0x5c`, and copy of its return into the helper input `r7`.
- `confirmed`: `0x82102568` receives the three reader scratch areas and two
  3-float auxiliary record pointers; it initializes the scalar output to `-1`.
- `cross-match`: mapping of the dynamic slot to the canonical `0x82100600`
  leaf and relation to the NDXR table.
- `unknown`: callback signature, record class, units of the three floats, and
  final gameplay/renderer meaning.

This is enough to define a safe ABI-level differential harness: preserve the
three scratch areas, the two record pointers, the two shifted indices and the
boolean result at both stages.  It is not enough to expose a public semantic
API.

## Evidence commands

Read-only headless dumps:

```text
DumpRange.java 0x82102340 0x82102430
DumpRange.java 0x82102568 0x82102620
DumpRange.java 0x821022d0 0x821023a8
DumpRange.java 0x82102180 0x82102230
```

The XenonRecomp cross-check is in
`.tools/recomp-eval/ac6/output/ppc_recomp.10.cpp`, functions
`sub_82102148`, `sub_82102568` and `sub_82102E70`.  No generated output or
native source was modified.

## Next bounded step

Inspect `0x822c2868`, which consumes the callback result plus the two
3-float-record pointers, and establish whether its output is copied into the
same scratch vectors.  Stop at an ABI-level contract if no stable type follows;
no human session is required.
