# Cycle 1204 — the container walk enters the product, and my control was weaker in C++ than in Python

## What was ported

`include/ac6/retail_ndxr_container.h` and `src/retail_ndxr_container.cpp`: the
NDXR **container** walk — recognise, GIDX unwrap, type code, four section
extents, the fixed `0x30` record array, and the string table — with the header
carrying the derivation stage by stage and the address that establishes each
(`0x8234CA28`, `0x8234CB58`, `0x82350C50/CA0`, `0x82352B88`, `0x82350F08`,
`0x823556E0`, `0x823555D0`, `0x82355318`).

**It stops at the polygon descriptor, deliberately.** Cycle 1203 showed the
product's existing reader and this derivation disagree there and that *neither*
field mapping is controlled. Porting a descriptor would have swapped a measured
guess for an uncontrolled one, which is not an improvement.

Two deviations from retail, both written into the header rather than assumed:

- retail **relocates the buffer in place** and guards with two bits; this reader
  takes a const buffer and resolves on use, so the guards are expected clear and
  the test asserts it;
- retail does not check `[+0x04]` against the file length; this does, because it
  reads untrusted bytes.

## The test failed, and the fault was mine

The new test asserts the discriminating control from cycle 1196 — the string
table is at `[+0x10] + 0x30`, and the rival without the `+0x30` must fail. In
Python that control read *sixteen* bytes and scored **0 of 537**. In C++ I wrote
"a NUL-terminated printable run", which **a single stray character satisfies**,
and the rival passed in **78 of 537 files**.

The derivation was never in danger. My re-implementation of my own control was.

Measured properly, with both curves in hand:

| minimum name length | derived base | rival base |
|---|---|---|
| 1 | 537 / 537 | **78 / 537** |
| 2 | 537 / 537 | 30 / 537 |
| **4** | 537 / 537 | **0 / 537** |
| 8 | 537 / 537 | 0 / 537 |
| 12 | 513 / 537 | 0 / 537 |

The threshold is **8** — inside the plateau where the derived base still names
every file and the rival names none. It was chosen from the table, not raised
until the test went green, and the table is in the test's own comment so the next
reader can see which it was.

**The lesson is narrower than "test your tests" and worth stating exactly: a
control re-expressed in another language is a new control and inherits none of
the first one's evidence.** `INSTRUMENT_DISCIPLINE.md` already records eight
false negatives and one true-positive-from-dead-code; this is a tenth shape — a
*weakened* control that still passes on the true hypothesis and therefore looks
healthy from the green side.

## Google C++ Style Guide

Applied to the three new files at the user's instruction: no macros (the test's
`REQUIRE` became a `Check` function returning `bool`), `CamelCase` for
non-accessor functions (`Open`, `Record`, `RefusalToString`, `Be32`), accessors
left snake_case as the guide permits, 80-column lines, `kConstantCase`.

**This diverges from the rest of the product**, which is uniformly snake_case
(`decode_ntxr_base_level`, `ModelDirectory::open`, `ModelDirectory::entry`). I
applied the guide to the new files rather than leaving them in the old dialect,
and did **not** sweep the existing sources: a rename across
`ntxr_texture`, `retail_model_directory`, `retail_scenario` and the raster target
touches every call site and every test in one commit, which is exactly the change
that should not ride along with a derivation. It is a separate piece of work and
it is recorded here as owed.

## Results

```
ndxr-container files=537 opened=537 refused=0 records=13014
  unnamed records               : 0
  relocation guard set on disk  : 0
  derived base names >= 8 chars : 537 of 537
  rival base (no +0x30)         : 0
```

Five predictions, four of which could have failed on the true hypothesis and one
of which — the rival — must fail for the derivation to mean anything.

## Not established, stated plainly

- Everything cycle 1203 left open: the descriptor field mapping, the vertex
  stride on either side, and what inserts into the id registries.
- This reader produces no geometry and is not wired into the renderer. It is a
  container walk with a test, not a mesh loader.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped, no DISPLAY)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
```
