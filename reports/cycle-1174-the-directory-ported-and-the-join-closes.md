# Cycle 1174 — the directory ported, and the join closes end to end

## What was written

`ModelDirectory` — `include/ac6/retail_model_directory.h`,
`src/retail_model_directory.cpp`. It is the three-word container `0x8228E988`
builds and the two accessors that read it:

```
8228e988  stw r4,0x0(r3)      ; word0 = the blob
8228e994  stw r11,0x4(r3)     ; word1 = blob + [blob+0x0C]
8228e9a0  stw r11,0x8(r3)     ; word2 = blob + [blob+0x10]
8228e9bc  lwz r11,0x4(r11)    ; the count, at blob+0x04
8228e9e0  add r3,r11,r10      ; entry = base + table[index]
```

Over Mission 01's `001_MDLP.mdlp`: **94 entries, and all 94 begin with `FHM `**.
A wrong table offset or a wrong base breaks that immediately, which is why it is
the check rather than a parse-succeeded assertion.

## The join, closed

```
model bindings carrying an index            311   (434 records less 123 sentinels)
indices the directory refuses                 0
distinct primaries / secondaries          38 / 38
```

Every model byte the scenario container carries addresses an entry this directory
serves. Container and directory are two files, read by two unrelated parsers, and
they agree on every one of 311 references.

## One thing this port does that retail does not

It **validates before serving**. Retail adds the table and base offsets to the
blob and indexes, because the archive is its own. This reads an untrusted file,
so a directory whose table or base runs past the end is refused at `open` rather
than served and bounds-checked per call — and the reason is written in the
header, so the divergence is on the record rather than looking like a
misreading.

It also computes an entry's **extent**, which retail never needs: retail hands
the pointer to a parser that reads its own length. A caller here wants a slice,
so `entry()` returns the distance to the next entry. That is additive, not a
different reading of the code, and it is marked as such.

## What it deliberately does not do

- **Open a file or resolve a name.** `0x820A85E0` finds the blob by descending
  the payload tree under a name hashed from `"DPL::[%#x,%#x]"`, and that lookup
  is not ported. The class takes bytes it is given.
- **Know what an entry contains.** `every_entry_starts_with` is a caller's check,
  not a promise about FHM.
- **Load anything.** This is the directory, not the loader. Nothing yet reads an
  NDXR out of an entry, and `mission_ready` is still false.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit_ac6_mission01_native_gate.py ... --require JF  ->  audit-valid JF=pass  (v3 and v4)
```
