# Cycle 1175 — the FHM layout is measured, so it stays out of the product

## The question

`ModelDirectory` reaches an entry; an entry is an `FHM ` bundle; a loader needs
to walk it to its NDXR. `tools/ac6_fhm.py` already does that in Python and
parsed all 94 of Mission 01's bundles with zero notes. Porting it to C++ is the
obvious next unit.

## Why it is not written

The Python parser's layout — count at `+0x10`, offsets at `+0x14`, sizes at
`+0x14 + 4·count` — has no derivation. Searching the image for the signature
`FHM ` built as an immediate finds nothing: the only two hits on `0x4648` are
`addi r10,r10,0x4648`, address arithmetic, not a magic comparison. FHM is
recognised the same way NTXR is (cycle 1149) — by container type code, not by
signature — and the reader that consumes its tables has not been found.

So the layout is measured: 94 of 94 bundles parse, offsets and sizes close, and
the reserved trailing slots behave as the parser's comment describes. That is
good evidence and it is not a reading.

Porting it would move a measured format description **into the product**, where
the auditor reads derivations and cannot tell the difference. The NTXR decoder
has two measured conventions in it — the record spacing and the terminator — and
each is a single constant with its measurement written beside it. An FHM walker
is not a constant; it is a format, and importing a format on measurement is how
a port stops being a derivation and becomes a reimplementation of a guess.

## What would unblock it

The reader. NTXR's was found by following the resource system from the type
dispatch (`0x8234CB58`) rather than from the magic, and the same route is open
here: `0x82343010` and `0x8234CB58` already handle typed resources, and the FHM
case is the one not yet followed out of them.

## What exists instead

The directory stops at the entry boundary, which is exactly where the derivation
stops. `ModelDirectory::entry()` returns an offset and an extent; a caller can
slice a bundle and hand it to whatever walks it. When the walker is derived it
drops in behind that interface without changing it.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product code changed.
