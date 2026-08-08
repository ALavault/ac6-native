# Cycle 1149 — the null model cut my own rule down, and 2d is not a porting job

## What this cycle set out to do

Step 2d of the ladder: port BC3 + Xenos `Tiled2D` + 8-in-16 out of
`scripts/probe_ntxr_bc.py` and into C++. It reads like the one item on JV that
needs no new retail discovery — the profile is validated, the script is 91 lines,
the formats are standard.

It is not a porting job, and the reason is in this repository's own report.

## The blocker is a refusal already on record

`NTXR_STRUCTURE_REPORT.md` says, of the descriptor words:

> it does not assign unproven width, height, tiling or compression semantics to
> their bit fields

> no field is yet named as width, height, format or mip count

But `probe_ntxr_bc.py` does exactly that:

```python
words = struct.unpack_from(">12I", data, 0x10)
width, height = words[5] >> 16, words[5] & 0xFFFF
start = 0x10 + words[8]
```

The script is honest about its own status — its docstring reads
*"Diagnostic-only BC/tile probe; native decoder is gated on visual proof"* — and
the report's validation is visual: an intelligible aircraft atlas, with a
negative control that omitting 8-in-16 corrupts the colours. Moving that
interpretation into the product as derived behaviour would promote a rule whose
support is that the picture looks right. That is anti-goal 2 verbatim.

So before porting anything I tried to earn the field names with measurement.

## The measurement, and the null model that cut it down

The corpus is 692 `NTXR` wrappers extracted under
`reports/logs/cycle-739-pac-mission-gate/fhm/`. Word 5's halves take 41 distinct
value pairs, 148 of them non-square, with 256×256, 64×64 and 128×128 dominating.

If word 5's halves are the dimensions, the BC3 byte count is `hi * lo` and the
payload should be a small clean multiple of it. Measured:

```
payload / (hi * lo) lands on an exact integer or half:   576 of 692   83%
```

with the mass on 12, 4, 2, 1, 16 and 8, and a tail at 1.3359 ≈ 4/3, which is
what a full mip chain costs.

**83% looked like strong evidence. It is not.** Shuffling the payloads against
the dimension pairs — same dimensions, same payloads, random pairing, 200 trials
— gives:

```
null model mean 70.9%,  max over 200 trials 73.3%
```

The baseline is that high because dimensions are powers of two and payloads are
large multiples of powers of two, so almost any pairing divides cleanly. The
observed 83% clears the null's maximum, so there *is* signal, but a 12-point
margin over a 71% floor does not name a field that a prior report deliberately
refused to name.

A control in the other direction confirms the test can discriminate at all: the
same measure on word 0 scores **0% over 250 usable wrappers**.

## The orientation test, which returned nothing at all

Even granting that word 5 carries the two dimensions, nothing above says which
half is width. `hi * lo` is symmetric, so the ratio test can never tell.

There is a non-visual test available: the `Tiled2D` address depends on the
pitch, so the wrong orientation should push some block addresses past the end of
the payload. Run over all 148 non-square wrappers, computing the maximum tiled
address reached for both orientations:

```
both orientations fit within the payload : 148
only hi16-as-width fits                  :   0
only lo16-as-width fits                  :   0
neither fits                             :   0
```

**Completely null.** The tiled address space, padded to 32-block tiles, is
symmetric enough that both readings stay in range for every wrapper in the
corpus. The test was worth running and it decided nothing, which is worth
recording so nobody runs it again expecting more.

## What 2d actually needs

The retail consumer. The descriptor is only nameable by finding the code that
reads it and programs the Xenos texture fetch constant from it.

A start on that, and one concrete fact:

- The image holds a family of four-byte signature predicates. `0x8233EF48`
  tests `0x4E445852` = `NDXR`; `0x8233EF68` tests `0x47494458` = `GIDX`. Both
  have the same shape — `lis`/`ori` the magic, `lwz` word 0 of the argument,
  `subf`, `cntlzw`, return a bool.
- **No instruction in the image builds `0x4E54`**, the high half of `NTXR`.
  Scanned across all 786,122 instructions. So `NTXR` is *not* magic-checked the
  way `NDXR` and `GIDX` are, and the sole occurrence of the literal, at
  `0x82067EC0` in `.rdata`, is not reached by that pattern.

That is the thread: NTXR wrappers are recognised some other way — by container
type code rather than by signature — and the code that consumes their descriptor
has to be found through the resource system, not through the magic.

## Decided rather than asked

**No C++ decoder is written this cycle.** BC3 and the `Tiled2D` swizzle are
standard formats and porting them is a morning's work, but a decoder is useless
without the header interpretation that tells it what to decode, and that
interpretation is exactly the part I could not earn. Writing the decoder now
would mean shipping the unproven field names inside it, where the auditor cannot
see them and the next reader would assume they were derived.

The alternative — port the decoder and mark the header parse diagnostic, as the
world markers are — was considered and rejected. The marker lane is honest
because markers make no claim beyond a position the container really states. A
texture decoded through guessed dimensions makes a claim about retail pixels in
the one domain where JV will eventually be measured against Xenia.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  24/24 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  mission01_final_gate=audit-valid JF=pass open=none
audit_ac6_class_map.py ... --require J2              ->  class_map=pass vtables=811 rejects=1619
```

No product code changed. This cycle spent its effort refusing to promote a rule,
and the null model is the reason.
