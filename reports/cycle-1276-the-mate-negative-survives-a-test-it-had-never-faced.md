# Cycle 1276 — the MATE negative survives a test it had never faced

## Qualification

Flat image `analysis-input/ACE6_X360.exe`. `default.xex` SHA-256
`acc302c1…11bcde`. **No oracle pass was spent.** No product code changed.

## Why

Cycle 1275 overturned a thirty-cycle negative — *"`0x82297540` has zero
instruction references"* — by looking for the one form no scan here covered: an
address built as `lis` + `ori`/`addi`. The obvious next question is which other
published negatives rest on the same blind spot, and the campaign's most
load-bearing one is a **magic constant**, which is exactly the kind of value
this binary materialises that way.

`0x8234B304 lis r9,0x4e54` / `ori r9,r9,0x5852` is how `'NTXR'` is tested. If
`'MATE'` were tested the same way, cycle 1207's *"MATE is never parsed"* would
be a false negative of precisely the shape just corrected.

## Established — it is not, and the controls are exact

```
0x4D415445  'MATE'   materialised at 0 sites
0x4E545852  'NTXR'   materialised at 1 site   0x8234B304
0x4E445852  'NDXR'   materialised at 1 site   0x8233EF48
0x821B5808           materialised at 0 sites
```

**Both positive controls land on the exact instruction already read.**
`0x8234B304` is the NTXR validator's magic test, transcribed in cycle 1256;
`0x8233EF48` is the NDXR magic test, transcribed in cycle 1261. An instrument
that finds both known cases at their known addresses and returns zero for the
third is telling the truth about the third.

`0x821B5808` returning zero is a second control of a different kind: cycle 1225
established it is reached *as a table entry* and appears in no instruction, and
it is not materialised either — consistent, and it confirms the scan does not
manufacture hits for an address that genuinely has none of this form.

So **cycle 1207's negative stands**, now with three independent instruments
behind it: the text scan, the byte scan, and the materialisation scan. MATE is
not parsed, and the conclusion the campaign built on it is unchanged.

## The corrections carried out

- **`MISSION01_LADDER.md`** and **cycle 1244's report** both carried
  *"`0x82297540` has zero instruction references"* as the placement chain's
  single open hop. Both are annotated in place with the six materialisation
  sites and the instruction that stores the address.

## Not established

- **The other four negatives in the repository were not re-run**, because they
  are byte strings rather than addresses or magics — `46 48 4D` in cycle 1192 is
  three bytes and cannot be materialised as a 32-bit immediate pair.
- **Whether any negative older than the instrument era rests on this blind
  spot.** The grep covered the phrasings in use; a cycle that worded a reference
  count differently would not have been found. Two greps is not a proof of
  absence, and this is the second time this session that sentence has had to be
  written.

## The shape

A negative overturned and a negative confirmed, by the same instrument, in
consecutive cycles. That is the useful pattern: **an instrument earns trust by
being run against what is already known before it is run against what is not.**
Had the materialisation scan been written without the NTXR and NDXR controls, a
zero for MATE would have been worth nothing at all — and it would have looked
identical.
