# Cycle 1225 — a tool for data words, and the first independent confirmation of entry 45

## The gap in the toolkit

Every scanner in `tools/ghidra_scripts` searches **instruction text**. A function
reached only through a vtable slot or a dispatch table is mentioned by no
instruction, so those scans return zero — correctly, and uselessly. Cycle 1224
walked straight into it: `0x821B5808` is the general mode-creator setter and
occurs zero times in 852,724 instructions, because it is a table entry.

`INSTRUMENT_DISCIPLINE.md` has said since cycle 1212 that a load-bearing negative
should be raised to a byte-level scan. **No tool here did that**, so the advice
could only be followed by hand, and mostly was not. Two subagents this session
wrote private decoders to do it.

`tools/ghidra_scripts/Ac6XenonFindWord.java` finds a 32-bit big-endian value as
data across every initialised block, at every byte offset rather than only
aligned ones, and reports both counts.

## What it answers immediately

```
blocks=12  bytes=11,117,714
  821b5808 = 2 aligned / 0 unaligned
  821b54c0 = 1 aligned / 0 unaligned
  821bbf98 = 2 aligned / 0 unaligned
```

```
821b54c0  at 0x8206551C  .rdata      <- a vtable slot
821b5808  at 0x820655CC  .rdata      <- a vtable slot
821b5808  at 0x8207EBA8  .pdata         (an unwind record, not a reference)
821bbf98  at 0x8207EF28  .pdata         (likewise)
821bbf98  at 0x82691B8C  .data       <- the creator table
```

**Both setters are vtable slots**, which is exactly the shape cycle 1224 inferred
from the zero and could not confirm. They sit `0xB0` apart in `.rdata`.

`.pdata` entries are function-range records, not references; the script reports
them and they must be discarded, which is the same trap a subagent noted when
`.pdata` produced spurious one-slot "vtable runs".

## The confirmation that matters

`0x821BBF98` — cycle 1218's `new CModeTaskGame` — is at **`0x82691B8C`**, which is
precisely the creator-table entry that cycle named as entry 45, and which cycle
1224 reached by arithmetic (`0x82691B8C − 0x82691ADC = 0xB0 = 44 × 4`).

Three routes now agree on that slot: a disassembly of the creator, an offset
computation from the setter's base, and a byte scan of `.data` that knew nothing
about either. **The first of those was a subagent's claim I had never
independently checked.** It holds.

## Not established, stated plainly

- **Which vtables `0x8206551C` and `0x820655CC` belong to, and at what slot.**
  That needs the run boundaries and the RTTI walk cycle 1218 described. The
  `0xB0` gap between them is suggestive of one vtable and is not evidence.
- Whether anything calls the slot that holds `0x821B5808` — the question just
  moves up one level, and this is where it now sits.
- The creator table's two-column alternation, unchanged from cycle 1224.
- Cycle 1216's `[0x82871084]` enumeration. Now trivially re-runnable with this
  tool, and still not run.

## On the pattern this session has settled into

Three of the last five cycles found a result by **building the instrument the
previous cycle needed and did not have**: force-disassembly scanning (1221), then
its use to void a negative (1223), now data-word search (1225). Each was
one file and under an hour, and each immediately answered something that had been
sitting open.

That is worth noting because the alternative — reasoning harder with the tools at
hand — is what produced cycles 1198, 1208 and 1214, all of which had to be
corrected. **The cheaper fix was usually the instrument.**

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
11,117,714 bytes across 12 initialised blocks
```

No product code changed.
