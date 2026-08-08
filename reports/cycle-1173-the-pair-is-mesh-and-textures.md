# Cycle 1173 — the pair is a mesh and its textures, and the split is perfect

## The test

Cycle 1172 ported the model binding and deliberately declined to say what the
second index is for: *"variant, damaged model, level of detail and shadow proxy
are all available readings and none is established."*

There is a test available that does not require reading more code. Mission 01's
directory holds 94 entries, and cycle 1157 walked every one: **47 carry geometry
and 47 do not.** If the two indices had interchangeable roles, both sets would
scatter across that partition in roughly the same proportion.

```
directory entries                                    94
  of which carry an NDXR                             47

the 38 distinct primaries    in a geometry entry     38 of 38
the 38 distinct secondaries  in a geometry entry      0 of 38
```

A perfect partition, in both directions at once.

So `primary` addresses the **mesh** bundle and `secondary` the **texture** bundle
beside it. That is why the two are consecutive, and why `primary` is almost
always even — the directory alternates.

## What kind of claim this is

A measurement over one mission, not a reading of the code. `0x820A7070` performs
two identical `0x8228E9B8` lookups and nothing in it distinguishes their roles;
both results go to the same binder. The distinction is visible in the *data*, and
the data is one mission's.

So the header records it as measured, with the numbers, and the struct still
carries the two bytes **without acting on the distinction**. `has_secondary()`
still only says whether the second lookup happens. A `mesh_index()` /
`texture_index()` pair of accessors would be the moment this stopped being
honest, and they are not written.

## Why the margin is worth stating

38 of 38 and 0 of 38 against a 47/94 partition is not a trend. If the indices
were assigned without regard to content, seeing all 38 primaries land in the
geometry half is about one chance in 2^38, and seeing the 38 secondaries land
entirely in the other half at the same time squares it. The measure was chosen
before the result was seen, and it had a clear way to fail — a single primary in
a texture-only entry would have killed the reading.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  25/25 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```

No product behaviour changed; the header states what was measured.
