# Cycle 1392 — the boundary, and the first fully-live composite

## Qualification

- **Ghidra project `ghidra-projects-xenon/ac6-xenon`**, `default.xex` SHA-256
  `acc302c1…11bcde`. **No oracle pass.**
- **Product C++ changed**: `retail_flight_export.h`, `.cpp`, its tests,
  `CMakeLists.txt`. **ctest is 43**, was 42.
- New tool `tools/audit_flight_export_microexec.py`, new artefact
  `analysis/flight/flight-export-microexec.tsv`.
- **Contract: the twenty-fourth behaviour**, `retail_flight_export`.

## The model's boundary

Callers of slots 17 and 18 are 100 and 184 dispatches unbounded; filtered to this
class family, **two each**, and they are the same two functions — slots **20 and
21** of the live vtable, which the base leaves as the empty `blr`.

They take a caller-supplied buffer and fill part of it with the model's control
outputs and a handful of raw fields. That is the boundary between the flight
model and whatever consumes it.

## And cycle 1391's open question is answered

That cycle noted there was no blend accessor for the middle axis and left it
there. **Slot 16 is two instructions** — `lfs f1,308(r3); blr`.

```
slot 16   [+308]                            raw
slot 17   blend of [+304], [+408], [+144]   contracted
slot 18   blend of [+312], [+412], [+152]   contracted
```

Two axes blended, one passed through. The absence was not an oversight in my
search; it is the design.

## Two details that a plausible port gets wrong

**Slot 20 calls the accessors in the order 16, 17, 18 and stores them at +12, +8,
+16.** The call order and the store order disagree. Visible only because both
were read; a port that assumed they matched would swap two of three outputs, and
the control `CONTROL assuming call order is store order must disagree` fires on
all sixteen sweep points.

**`[+376]` is written twice by slot 20** — to `+28` and to `+52`. Not a
transcription slip: `0x823038F8` and `0x82303920` both load it and both store it.
Slot 21 also writes `+52`, from a *different* source, so the two exporters
overlap on that one word and disagree about it. Pinned.

## The first fully-live composite

```
flight_export_microexec=pass cases=4 values_compared=128
```

Every earlier composite in this campaign stubbed something — cycle 1381 stubbed
slots 31 and 32, cycle 1389 stubbed everything but slot 30. **Here nothing is
stubbed.** The synthetic vtable holds the genuine addresses of slots 16, 17 and
18; all three run for real through real virtual dispatches; and `calls` is
asserted **empty**.

So `retail_control_blend` is exercised through the dispatch that actually reaches
it, not through a Python restatement of it.

**And the whole 128-byte buffer is compared**, not the words the exporters write.
It is seeded with −1.0 everywhere, so every offset a given exporter does *not*
write is asserted still to hold −1.0. A port that zeroed the buffer first, or
wrote one word too many, fails on the untouched offsets rather than on the
written ones — which is the failure mode a write-set comparison cannot see.

## Not established

- What calls slots 20 and 21, and what the buffer is. `+0` and `+4` are never
  written by either, so the buffer is larger than what the flight model fills.
- Slot 16's other overrides: `0x8200FC28` replaces it with `0x82329448`.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 32 (1351–1371, 1374, 1376–1379, 1382–1386) |
| implementation/integration spent on A3.2 | 14 (1354–1356, 1372, 1373, 1375, 1380, 1381, 1387–1392) |

Seven consecutive cycles ending with a contracted behaviour, and the contracted
chain now runs from the per-frame step to the buffer the rest of the game reads.

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 24 behaviours
ctest                                 100% passed, 0 failed out of 43
tools/tests                           Ran 77 tests, OK
flight_export_microexec               pass 4 cases, 128 values, nothing stubbed
```

## Next

What calls slots 20 and 21. The buffer's `+0` and `+4` are written by someone
else, so the caller is the owner of a structure the flight model only partly
fills — and finding it is where the flight model meets the rest of the game.
Same bounded dispatch search, offsets 80 and 84.
