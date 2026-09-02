# AC6 retail NTSC-U/J — documentation only: no static resolver for the r224 table; 2 of 14 fallback entries have real direct callers, 12 have none (r225)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No code change this cycle** — documentation only, same class
as r164/r178/r202/r211/r212/r218/r219/r220/r221/r224. `ctest` 10/10
(`build/ntsc-uj/native/native-cmake`), pytest 205/205 (204 passed + 1 skip),
both reproduced fresh this cycle since source was untouched.

## What was found

r224 left an explicit next step: find the resolver function that reads the
≥14-entry `{function_address:uint32, tag:uint32}` fallback table at file
offset `0x7ffa0` (VA `0x821f44d8`), by analogy with `Function_821FCCE0` from
r212. Two candidate approaches were considered: continuing raw byte-pattern
search for a `lis`/`addi` materialization of the table's own base address
(already tried and negative in the prior tick), or querying Ghidra's
`ReferenceManager` directly by address. `scripts/FindSymbolReferences.java`
cannot do the latter — it only iterates existing named `Symbol` objects and
substring-matches, so it is blind to any of these unlabeled trampoline
addresses. `scripts/ReferencesTo.java` already exists in this project's
`scripts/` directory and does exactly this (`getReferencesTo(Address)`,
symbol-independent) — it did not need to be written.

Running it against the table's own base address (`0x821f44d8`) and each of
the 14 listed entry addresses found:

- **12 of 14 entries have zero references** anywhere in the XEX — no direct
  `bl`, no `bctr` Ghidra's static analysis resolved, nothing.
- **`0x821f4680`** (r224's `XamShowMarketplaceUI` candidate, tag
  `0x40003003`) has exactly **one** real reference: `82140d10
  UNCONDITIONAL_CALL`, an ordinary direct `bl`.
- The table's own base address (`0x821f44d8`) has **zero** references —
  nothing in this XEX materializes the table's start address as a pointer
  anywhere Ghidra's reference manager can see, static or dynamic.

Decompiling the caller of `0x821f4680` (`Function_82140AF8`, containing the
call site `0x82140d10`) also revealed a second, closely adjacent address,
`0x821f4678`, called from two real sites in this same function (`0x82140c10`)
and from an unrelated function `Function_8215CB88` (`0x8215cbc4`).
**`0x821f4678` is not one of the 14 table entries** — the nearest listed
entry is `0x821f4660` (tag `0x40000703`), which itself has zero references.
`0x821f4678` sits between two table entries in the same small unlabeled code
region (`symbol-before`/`symbol-after` for all three addresses resolve to
the same enclosing `Function_821F4618`/`Function_821F46A0` bracket — Ghidra
has not split this region into individually named functions).

Both real call sites of `0x821f4680` and `0x821f4678` follow the same shape:
two arguments (a small integer, read from `*(iRam8293b930+0x84)` — the same
per-title user-index-like field already read by several already-fixed
imports; and an 8-byte value read from an unrelated struct at `+0x10`), and
a return value checked strictly against `0` before setting a one-shot
"handled" flag at a fixed struct offset (`+0x1f0` for `0x821f4680`, `+0x1ec`
for `0x821f4678`).

## Consequence: r224's "resolver function" framing does not hold up

r224 assumed a single loop-based resolver reads this table and `bctrl`s to
whichever entry matches a requested ordinal/tag, the same shape r212 found
for `XexGetModuleHandle`-gated resolution. **No evidence supports that
here.** The table's own base address has no references at all — nothing
loads it as a base pointer for indexed access anywhere this static pass can
see. What actually exists is simpler and different: a handful of these
addresses (so far, exactly 2 of the ~16 candidates checked) are called
**directly** by ordinary fixed `bl` instructions from specific, unrelated
call sites, exactly like every other statically-linked internal helper in
this XEX — not through any table-driven indirection. The table itself may
still be a real data structure (e.g., an ordinal/version compatibility
descriptor consumed by something other than code, or read only via an
indirect branch this `-noanalysis` pass cannot resolve), but **there is no
static evidence of a resolver function reading it**, and continuing to
search for one is not justified by what has actually been found.

This also means: whatever `0x821f4680`/`0x821f4678` implement, they are
**not** offline-import stub gaps in the sense this sweep targets. Neither
address carries a Ghidra symbol at all — `materialize_native_import_stubs.py`
only ever operates on named kernel/XAM imports resolved through this
project's synthetic import-stub table; these are ordinary internal `.text`
addresses of the qualified XEX, already subject to ordinary guest-code
recompilation by XenonRecomp like any other internal function, not routed
through the offline-stub mechanism this sweep is closing gaps in. There is
no `render_body()` case to add for them, and asserting an import name for
either (`XamShowMarketplaceUI` was r224's guess, from the tag pattern and
table position, never confirmed by a symbol or by tracing what the function
body itself does) would be exactly the kind of unread assertion this
project's evidence discipline refuses.

## Why this still isn't implemented, and why it may not need to be

Twelve of the fourteen table entries are, as far as this static pass can
tell, genuinely unreached in this build — consistent with the same
"marketplace/sign-in UI paths not exercised by Mission 01 gameplay" pattern
already established for the rest of the `XamShow*` cluster (r206/r220).
`0x821f4680`/`0x821f4678` are real and reachable, but they are recompiled
guest code, not stub imports — if either turns out to matter for Gate 2 it
will surface as a behavior gap in the recompiled `.text`, not as a
`kOfflineStatus` stub gap, and belongs to a different investigation than
this sweep.

## Next

1. Treat the r224 "find the resolver" lead as closed: there is no static
   resolver to find with the evidence available. Do not spend further
   cycles searching for one under `-noanalysis`.
2. This entire dispatch-table thread is **out of scope** for the
   offline-import stub sweep — it does not gate any named import. Remove it
   from the sweep's active lead list; if `0x821f4680`/`0x821f4678` ever need
   attention, that is a recompiled-`.text`-behavior investigation, not a
   `materialize_native_import_stubs.py` fix.
3. Continue the broader offline-import sweep per r218's bucket catalog —
   this cycle found no new stub-import fix, so the offline-generic-import
   count is unchanged from r223's checkpoint.
4. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
