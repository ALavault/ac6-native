# Cycle 1385 — a named class, at last

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus, the image, and
  `analysis/class-map.tsv`.
- No product C++ changed; ctest stays 37. **No contract entry** — research.
- New artefact `analysis/flight/unit-factory.tsv`.

## The factory is a method of a class with a name

`sub_820A8138` has **no direct callers** — the third function in this thread with
that property, and for the same reason. `tools/whose_vtable.py` places it at slot
**`+0x14`** of the vtable `0x82055190`, and that vtable **is named** in the
campaign's own class map:

> **`ACE6::CX360UnitManager`**

This is the first named anchor the flight thread has had. The entity, both flight
models, the integrator, the control surfaces, the orientation update — all
unnamed, all of them among the 306 vtables without RTTI. The manager above them
is not.

Worth saying how it was found: one call to a tool that has existed the whole time,
against an address I already had. Cycle 1370 established the 306 unnamed vtables
and cycle 1383 was caught not checking what the repository already answers; this
is the same lesson producing a result instead of a correction.

## The four kinds

`r4` selects through a jump table at `0x820A832C`, decoded **from the image** —
the disassembly renders those four data words as `lwz r16,…`, which is how cycle
1370 first read them.

| `r4` | constructor | size |
|---:|---|---:|
| **1** | `0x8222BEC8`, or `0x82293C28` when `[x+19568]` is non-zero | **10,672 / 12,160** |
| 2 | `0x8229D9B0` | 592 |
| 3 | `0x822A47E8` | 928 |
| 4 | a fifteen-row table keyed on `r5` | — |

Only kind 1 builds an object large enough to carry a flight model — the flight
model alone reaches `[model+1296]`, and 592 bytes cannot hold one.

## Which settles the scope of cycle 1384

Yesterday's finding was that the `0x8200F270` instance at `entity+2224` is never
addressed and the live model is the `0x8200F310` branch. The open question was
whether that entity is the player's aircraft or some side case.

Kinds 2, 3 and 4 **cannot** carry a flight model. Kind 1 is the only candidate.
So the finding applies to whatever flies, and the four contracted behaviours
describe a model that is not it.

I will not go further than that. Cycle 1384 was caught by exactly the move of
turning "this is the only class that *can*" into "this is the class that *does*",
and repeating it one level up would be the same error with a bigger object. What
is established is the negative: kinds 2, 3 and 4 are out.

## The fifteen-row registrar

Kind 4 searches a table in BSS at `0x82A218A0`, filled by this same function on
its first call under a flag bit: fifteen rows of `{key, fn, extra}` with keys
0…14 and `extra` values 4, 5, 10, 6, 8, 7, 9, 2, 13, 14, 15, 16, 17, 6, 4. It
calls `row.fn` and stores `row.extra` at `result+184`.

That is the **integer-keyed registrar** shape the campaign's JV decision
describes — "port the registrar and its integer-ID registry, not a directory
walk" — appearing here in an unrelated subsystem. Noted for that thread, not
pursued in this one.

## Not established

- Which kind the **player** gets as opposed to the AI units.
- What `[x+19568]` selects between the 10,672- and 12,160-byte forms.
- Anything about `CX360UnitManager` beyond this one slot.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 31 (1351–1371, 1374, 1376–1379, 1382–1385) |
| implementation/integration spent on A3.2 | 8 (1354–1356, 1372, 1373, 1375, 1380, 1381) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 18 behaviours
ctest                                 100% passed, 0 failed out of 37
tools/tests                           Ran 77 tests, OK
```

## Next

`0x82306A38` — the step that actually runs. It is 123 instructions, no vector,
six calls, and it dispatches slots 30, 31, 32 and 39 of the live model plus
`0x82282938` and `0x82326FE8`, two functions the *contracted* step also calls. So
the two drivers share their tail, and whatever is already understood about
`0x82283898` carries over.

Its slot 30 is `0x82303E68` and its slot 32 is `0x82306038`, both unread; slot 31
is `0x823042D0`, the 505-instruction vector aerodynamics, which the estimate
boundary of cycle 1383 puts out of reach of a differential. **Bound each by
capsule footprint before reading it** — that is what made cycle 1375 cheap — and
expect slot 31 to be portable only as far as its scalar prologue.
