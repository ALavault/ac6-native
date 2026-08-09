# Cycle 1322 — one value is not a test of a field

## Qualification

- Ghidra project `ghidra-projects-xenon/ac6-xenon`.
- `default.xex` SHA-256 `acc302c1…11bcde`.
- **No oracle pass was spent.** No console emulator, no bridge, no game run.
- No product C++ changed. **The port did not happen this cycle** — see *Next*.

## The correction, to yesterday again

Cycle 1321 swept `device+0x38` and `device+0x3A`, saw no flag bit, and wrote
that they "do not reach the flag word". **They do.** They own record flag bits
14 and 15 and fill the float slots at `+0x44` and `+0x48` — the two slots cycle
1318 could not place and guessed the triggers for.

The sweep drove them to `0xFFFF`, which as an `int16` is **−1**, and a negative
value never sets the bit. One value is not a test of a field, which is the same
sentence cycle 1304 wrote about `vrlimi128`'s immediates and cycle 1321 did not
apply to its own new cases.

## What the two fields do

Four values, one case each:

| written | as `int16` | slot | equals |
|---|---:|---:|---|
| `0xFFFF` | −1 | −0.000031 | −1 / 32767 |
| `0x4E20` | 20000 | 0.610370 | 20000 / 32767 |
| `0x8000` | −32768 | **−1.000031** | −32768 / 32767 |
| `0x0100` | 256 | 0.007813 | 256 / 32767 |

**`slot = float(int16(field)) / 32767`** — sign-extended, no bias, no split into
halves. `−1.000031` is the tell: a clamped or half-range normalisation cannot
produce a magnitude above 1.

That is a different rule from the one the eight axis halves take, in the same
record, into adjacent slots.

## The flag bit is not the slot

A reading would very likely have merged these. Execution separates them:

- the **slot** is written for every value above, negative ones included;
- the **flag bit** is set only for positive values **at or above 31**.

| value | slot | flag bit |
|---:|---:|---|
| 30 | 0.000916 | not set |
| 31 | 0.000946 | **set** |
| −32768 | −1.000031 | not set |

Bracketed by seventeen cases between 1 and 255. It is **not** a sign test — 1
does not set it — and **not** the `0x800` deadzone the other path uses — 256
does. Whether the comparison is an integer test against 30 or a float test
against a constant in `(0.000916, 0.000946]` is **not established**: both fit
every value measured, and I am not going to pick one.

## A hypothesis of mine, refuted by its own control

Cycle 1315 read a **second** normalisation at `0x821CB244` — subtract `0x4000`,
threshold at `0x800`, multiply by `float32(1/16383)` — which is not the one the
axis stage takes. `0x7FFF − 0x4000 = 16383`, so that path maps a full-scale raw
thumb onto exactly 1.0, and the four raw thumbs at `device+0x3C..+0x42` sit in
the snapshot unexplained. It was a good hypothesis and I wrote it into the tool
as one.

**All four raw thumbs reach nothing** — no flag bit, no float slot — in the same
batch where `halfLY` filled `+0x50` and fourteen button bits lit up. The control
was in the run, so the negative is a measurement.

So the biased path stays unreached and unexplained, and it is now known **not**
to be fed by the raw thumbs.

## The instrument grew a second reader, with the reason between them

`float_slots()` is **strict**: a slot whose four bytes are not all present was
not written, and is omitted rather than read as `0.0`. `flag_word()` defaults
missing bytes to **zero**. They are opposite rules in the same file because a
mask byte of zero is a value and a float of `+0.0` is indistinguishable from an
absence. Both readers carry that reason in the source.

The null control matters here too: it shows `+0x44` and `+0x48` at `+0.0` while
the four axis slots sit at `−0.0`, which is the first sign in the data that the
two groups take different code paths.

## Not established

- Whether the flag-bit threshold is integer or float.
- What `device+0x38` and `device+0x3A` **are**. They are placed, measured and
  unnamed; the earlier stage that writes them has not been read.
- What reaches the biased normalisation at `0x821CB244`.
- The consumer is executed and fully measured, and **not ported**.

## Gates

```
mission01_final_gate (playable-v1)   JF=pass open=none
ctest                                100% passed, 0 failed out of 28
contract_addresses                   pass, 144 cited, 144 supported
contract_derivations                 pass, 27 behaviours, 0 gaps
tools/tests                          Ran 72 tests, OK
instrument_discipline_index          pass, 19 shapes, 0 unindexed
contract_artifacts (live contracts)  pass, 50 cited, 50 match HEAD
```

## Next — and it is a redirection, taken from instruction

The port of the input record is deferred. The next tranche is `0x822A1E80`,
under a correction I accept: **it is one shared boundary, not two risks.** The
previous cycle's report called it a doubling because it carries both flight
orientation and rendered-unit orientation; that is only true if A3 and JV are
allowed to grow independent matrix conventions, which is a thing to forbid
rather than a cost to forecast.

Concretely, and none of this is a VMX opcode hunt:

1. **Drop the identity expectation as an assumption.** Zero angles imply an
   identity output only if the function builds an absolute rotation into a
   distinct destination with unit scale, no basis change and no model-base
   rotation. `Mout = Msrc × R`, `R × Msrc`, `B⁻¹RB` and `Mbase × R` all return
   something else at zero, and cycle 1304 already corrected cycle 1303 for
   calling the identity "the known answer".
2. **Five capsules**: identity source; a fully asymmetric 4×4 source; pitch-,
   yaw- and roll-only at 0.25, −0.5, 0.75; a translation sentinel `[17,29,43]`;
   and distinct / aliased / partially overlapping destination.
3. **Compare architecture before meaning** — basic blocks, load and store
   addresses and sizes, final GPR and VR state, raw destination bytes — through
   both the Ghidra micro-executor and the XenonRecomp generated C++ for this
   XEX, stopping at the first divergent architectural write. Sixteen words
   printed as a matrix is an interpretation and comes last.
4. **One native boundary**, `RetailTransformKernel`, shared by flight
   orientation and rendered-unit orientation, behind a gate until the function
   is qualified. Not `FlightOrientation::build_matrix()` and
   `RenderedUnit::build_matrix()`.

The question is no longer "which VMX instruction is wrong". It is **what
`0x822A1E80` computes, from which inputs, under which composition convention**.
