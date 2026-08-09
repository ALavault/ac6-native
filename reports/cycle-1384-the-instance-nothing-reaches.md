# Cycle 1384 — the instance nothing reaches

## Qualification

- **No Ghidra run and no oracle pass.** The recompiled corpus and the image.
- No product C++ changed; ctest stays 37. **No contract entry, and none
  withdrawn** — see below.
- New artefact `analysis/flight/which-model-is-driven.tsv`.

## The candidate was not a registration

Cycle 1383's open question was what steps the `+2224` object, with the base
constructor's call to `0x82282090` at `this+544` as the candidate. It is 99
instructions of `stfs f0,N(r3)` — a field initialiser. So is `0x822A5FA8` at
`+428`. Neither registers anything.

## Nothing addresses that subobject at all

The earlier scans looked for `addi rX,rY,2224` — a single displacement off a
tracked `this`. This one tracks **every** register that ever equals `base + k`
for **any** base, following `addi` chains, across the whole corpus.

Three sites, and they are the same three:

```
0x8222BF94  sub_8222BEC8   the constructor
0x82227BE4  sub_82227B08   the destructor
0x821F5AE4  sub_821F5AD8   a STRING address, 0x820008B0
```

And no store of that value exists: the entity's only flight-model pointer is
`+4912`, written twice in the whole corpus, both in constructors, both with the
**other** object.

## The other one is driven

Dataflow from `lwz rX,4912(rY)` to a virtual dispatch:

| slot | what | sites |
|---:|---|---:|
| 9 | the data load `0x82303D18` | 1 |
| **15** | **the F310 step `0x82306A38`** | **2** |
| 29 | the reset `0x82303A20` | 1 |
| 11 | the base step `0x82283898` | **0** |

And the entity is not itself a flight model: its vtable runs to 89 slots and
shares **zero** of 36 with the flight-model base, so there is no inheritance path
by which slot 11 could reach the model through the entity.

**For this entity, the live flight model is the `0x8200F310` branch.** The
`0x8200F270` instance at `+2224` is constructed, destructed, and never addressed.

## What this does and does not mean, stated carefully

Cycle 1379 concluded *"the class this campaign ported is genuinely a class that
flies"* because `0x8200F270` has a non-empty slot 11 while the sibling overrides
it with the empty `blr`. That is an argument about the **class**. I extended it to
the **instance** without noticing I had, and the instance is unreachable.

What is untouched: the four contracted behaviours —
`retail_flight_controls` (`0x82302DB0`), `retail_flight_step` (`0x82303110`),
`retail_flight_orientation` (`0x82302C88`) and `retail_flight_step_driver`
(`0x82283898`) — are **faithful ports of real retail functions, each verified
against the executed instructions**. Their contract statements say the product
reproduces those functions, and it does. Nothing here weakens that, and no entry
is withdrawn.

What is now open is their **relevance**. They are `0x8200F270`'s implementations.
The live model uses `0x8200F310`'s: slots 30, 31 and 32 are `0x82303E68`,
`0x823042D0` and `0x82306038`, and the step is `0x82306A38` — four different
functions, none ported.

I am not going to dress that up. Six cycles of A3.2's eight implementation cycles
went into a class whose instance nothing reaches, and the reason it was not caught
earlier is that every check asked "does this class fly?" and none asked "is this
object ever passed to anything?".

## The check that would have caught it, and it is cheap

**Before porting a class's methods, establish that some instance of it is
reached.** Two questions, both one search:

1. does any code compute the address of the subobject, by any chain?
2. does any pointer field ever hold it?

For `0x8200F270` both answers are no, and both were answerable at cycle 1370 —
which *did* run the first search, got three sites, and read the result as "the
component is reached through a stored pointer" rather than as "there is no such
pointer". The search was right; the conclusion assumed the object must be
reachable because it existed.

## Not established

- **Whether the player's aircraft is this entity at all.** The 10,672-byte object
  comes from the factory `sub_820A8138` on type ids 1..4, and which id the player
  gets has never been read. This now matters more than anything else in A3.2: if
  the player is a different entity type, the whole question reopens one level up.
- Whether `0x8200F270` is vestigial — a flight model kept for a mode that ships
  disabled — or reached in some build configuration.

## Two estimates

| | cycles |
|---|---:|
| research spent on A3.2 | 30 (1351–1371, 1374, 1376–1379, 1382–1384) |
| implementation/integration spent on A3.2 | 8 (1354–1356, 1372, 1373, 1375, 1380, 1381) |

## Gates

```
mission01_final_gate (final-v3)       JF=pass open=none
mission01_final_gate (playable-v1)    JF=pass open=none, 18 behaviours
ctest                                 100% passed, 0 failed out of 37
tools/tests                           Ran 77 tests, OK
```

## Next

**Which entity the player is.** `sub_820A8138` is the factory, it switches on a
type id in 1..4 through a jump table at `0x820A832C`, and its four branches build
objects of 10,672, 12,160, 592 and 928 bytes. Finding which id the player's
aircraft takes settles both this question and the one cycle 1370 opened, and it
is a bounded read of one 297-instruction function and its callers — the same
`0x820A8678` the plan named as where the aircraft comes from the profile.

Then: `0x82306A38` and its slots 30/31/32 are the functions that actually fly the
aeroplane, and three of the four are unread.
