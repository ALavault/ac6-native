# AC6 retail NTSC-U/J — documentation only: the `XamShow*` trampolines are the same dynamic-resolution fallback table r212 already found (r224)

Date: 2026-09-02/03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. **No code change this cycle** — documentation only, same class as
r164/r178/r202/r211/r212/r218/r219/r220/r221. `ctest`/pytest unaffected:
205/205 pytest, 10/10 ctest (last verified at r223, unchanged since no
source was touched).

## What was found

While re-checking whether the one-instruction `XamShow*`/`XamEnumerate`
tail-jump trampolines (r206/r212's "untraced" bucket) have any real
caller, direct symbol/call-reference search
(`scripts/FindSymbolReferences.java`) found none — as expected, since a
single-instruction `b <target>` fragment with no `bl`/`bctr` reference
anywhere nearby is not something Ghidra's static call-graph analysis can
resolve on its own.

A raw byte search of the qualified XEX for each trampoline's own address,
stored as a big-endian 32-bit value, found a hit for
`XamShowMarketplaceUI`'s trampoline (`0x821f4680`) at file offset
`0x7ffa0`. Reading the surrounding bytes reveals a real, extensive data
table of `{function_address: uint32, tag: uint32}` pairs — at least 14
consecutive entries, all with the same `0x4000____` tag pattern:

```
0x821f44d8 0x40001803
0x821f4550 0x40002203
0x821f45d8 0x40001403
0x821f4660 0x40000703
0x821f4680 0x40003003   <- XamShowMarketplaceUI's own trampoline address
0x821f4768 0x40000a03
0x821f4798 0x40001203
0x821f47e0 0x40001f04
0x821f4878 0x40008003
0x821f4a78 0x40002403
0x821f4b38 0x40002504
0x821f4bd0 0x40007d03
0x821f4dc8 0x40001a04
0x821f4e30 0x40006103
```

This is the same shape r212 already analyzed for
`XexGetModuleHandle`/`XexGetProcedureAddress`: a title probes for a
newer dynamically-resolved XAM export (gated by a version check), and
falls back to a fixed, statically-linked address when the dynamic
resolution fails — which, per r212, it always does in this build, since
`XexGetModuleHandle` unconditionally fails. **This table is the list of
those fallback addresses.** `0x821f4680` being both a table entry and a
trampoline this cycle was investigating confirms the connection directly
rather than by inference alone.

## Consequence: these trampolines are reachable, not orphaned

r206 and r212 both characterized the untraced `XamShow*` trampolines as
needing individual caller tracing to determine a safe default,
implicitly treating "no direct `bl`/`bctr` reference found" as an open
question about reachability. This table resolves that question: they
**are** reached, through exactly the same "dynamic resolution always
fails, so always fall back to this static address" path r212 already
confirmed is the live, executed path in this build (not the dynamic
side, which never activates). This is a **correction to prioritization**,
not a correction to any prior fix — nothing was implemented incorrectly,
but the trampolines should not be treated as low-priority "maybe dead
code," they are confirmed live call targets whenever the corresponding
higher-level game feature (marketplace UI, sign-in UI, etc.) is invoked.

## Why this still isn't implemented

Confirming reachability does not by itself determine what each
individual `XamShow*` import needs to return safely — that still
requires tracing each fallback target's own caller (the resolver
function that reads this table and performs the `bctrl`, plus whatever
calls *that* resolver for each specific ordinal) the same way r221/r222
resolved `XamShowMessageBoxUIEx`. This table has at least 14 entries;
mapping each to its named XAM import and tracing its own real
call-and-consumption pattern is a multi-cycle undertaking in its own
right, not attempted further here.

## Next

1. A future cycle tackling this bucket should start from this table
   (file offset `0x7ffa0` and surrounding entries) rather than
   re-discovering it, and should first identify the resolver function
   that reads it (the equivalent of `Function_821FCCE0` from r212, but
   for this larger, multi-entry table) to find each entry's own real
   caller.
2. `XamShowMessageBoxUIEx` (r206/r220/r221/r222) was resolved via direct
   `bl` tracing, not through this table — it is architecturally
   different from the trampoline cluster this table explains, worth
   noting so a future cycle doesn't conflate the two mechanisms.
3. Continue the broader offline-import sweep per r218's bucket catalog
   in the meantime.
4. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
