# Cycle 168 — reader ABI and output-lifetime contract

Date: 2026-07-18 (Europe/Paris)

## Target

Canonical AC6 Xbox 360 PAL `default.xex`, target ID `ac6-xbox360-pal`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`, Ghidra
project `ace-combat-6`, image base `0x82000000`.

This pass is static and read-only.  It uses the canonical `.pdata` boundaries,
the XenonRecomp output and the raw PPC call-sites.  It does not assign a
gameplay name to any value.

## Reader entry contract

The body `0x82102148..0x82102567` saves its incoming registers as follows in
the XenonRecomp mapping:

```text
r3  -> receiver, retained as r31
r4  -> pointer to a 16-byte output vector, saved at stack +428
r5  -> pointer to a 16-byte output vector, saved at stack +436
r6  -> scalar output pointer, saved at stack +444 and initialized to -1
r7  -> auxiliary input pointer, retained as r14
r8  -> auxiliary input value/pointer, saved at stack +460
r9  -> signed coordinate/index input (shifted by 4)
r10 -> signed coordinate/index input (shifted by 4)
```

The receiver checks `+0x28`, `+0x30` and `+0x5c`, bounds the derived indices
to `0..15`, and indexes the zone selected by the receiver's `+0x30` field.
The body later uses the retained `r7`/saved `r8` as arguments to an indirect
callback around `0x821023a4`; they are not shown to be output destinations.

## Call-site `0x82103010`

Inside the complete `.pdata` body `0x82102e70..0x821033a7`, the call prepares:

```text
r3 = r19                 # receiver
r4 = r1 + 0x80           # scratch vector A
r5 = r1 + 0x90           # scratch vector B
r6 = r1 + 0x60           # scratch scalar
r7 = r30                 # auxiliary input
r8 = r29                 # auxiliary input
r9 = r31                 # coordinate/index
r10 = r27                # coordinate/index
bl  0x82102148
```

On a true return, the caller loads the two 16-byte scratch vectors and stores
them into the external destinations held in `r25` and `r21`; it loads the
scratch scalar and stores it through `r23`.  The return is first reduced to
its low byte and tested as a boolean.  If false, these copies are skipped.

## Call-site `0x82103228`

The second call uses the same receiver, auxiliary inputs and coordinate/index
values, but adds a fifth scratch pointer:

```text
r3 = r19                 # receiver
r4 = r1 + 0x80           # scratch vector A
r5 = r1 + 0x90           # scratch vector B
r6 = r1 + 0x60           # scratch scalar
r7 = r1 + 0xa0           # additional scratch/context pointer
r8 = r1 + 0x70           # additional scratch/context pointer
r9 = r31                 # coordinate/index
r10 = r27                # coordinate/index
bl  0x82102148
```

The first three scratch locations have the same copy-back path to `r25`, `r21`
and `r23`.  The `r7`/`r8` locations are passed into the reader but no direct
copy-back to an external destination is established by this caller.  They
must remain neutral `auxiliary_scratch_*` names until the callback and
subsequent `0x82102568` path are correlated.

## Qualification

- `confirmed`: receiver, three output-pointer positions, two coordinate/index
  inputs, boolean return handling, and copy-back lifetime at both call-sites.
- `confirmed`: `r7` and `r8` are retained as auxiliary arguments inside the
  reader; they are not proven output pointers.
- `cross-match`: relation of these output vectors to the table records and to
  the candidate vtable/address-points.
- `unknown`: element types, units, class name, and meaning of the scalar
  result.  Do not call the vectors positions, normals, tiles or aircraft
  fields without independent proof.

The reader's direct contract is now sufficiently constrained for a native
adapter or a differential harness, but not for a semantic gameplay rename.

## Evidence sources

```text
.tools/recomp-eval/ac6/output/ppc_recomp.10.cpp
  sub_82102148
  sub_82102E70
```

The generated file was read only.  No generated source, Ghidra project, XEX or
native runtime was modified.

## Next bounded step

Trace the two auxiliary arguments through the indirect callback at
`0x821023a4` and compare its output with the `0x82102568` follow-up call.  If
that path cannot distinguish a type, retain the ABI-only contract and move to a
different high-value AC6 boundary rather than inventing a semantic name.  No
human session is required.
