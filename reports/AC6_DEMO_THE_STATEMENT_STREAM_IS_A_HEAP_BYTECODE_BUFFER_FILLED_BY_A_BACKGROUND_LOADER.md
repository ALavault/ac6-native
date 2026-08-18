# The statement stream is a heap bytecode buffer, filled by a bit-unpacking loader on a background thread

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence, four probe runs, all
`probe --until frontend --max-ticks 3200`, correctly-timed START, no
oracle, same fresh store each time (address-deterministic across all
four): (1) a full-log grep of `4ee47a17`'s own `AC6_SWG_RECORD_KEY_CALL`
capture; (2) `AC6_DEMO_WATCH_ADDR_LO/HI` bracketing the category-`0xB`
record's own heap address, combined with `AC6_SWG_RECORD_KEY_CALL` and
`AC6_SWG_LOOKUP_KEY`; (3) a new trace, `AC6_DEMO_WATCH_SWG_BOX_CALL`,
gated on `guest_address==0x820DA488U`, run combined with
`AC6_SWG_LOOKUP_KEY`, then extended to also read `[r31+20]`; (4)
`AC6_DEMO_WATCH_ADDR_LO/HI` bracketing the bytecode buffer itself.
Static: `sub_820D4A30`, `sub_820DA488`, and `sub_823246C0`'s full bodies;
`tools/whose_vtable.py` against `0x820DA488`; `strings`/`grep -abo`
against `.build/Default.xex.base.bin` and the store's `DATA00.PAC`/
`DATA01.PAC`/`DATA.TBL`.

## What this corrects in `4ee47a17`

**Five call sites reach `sub_820DEDF8`, not three-to-four.** A full-log
grep (`4ee47a17`'s own 4477-line capture, sampled at commit time) finds a
fifth, `lr=0x820E14E8` (69 occurrences, scattered ticks plus a 23-call
burst at tick 3033), missed because the report's live reading only
inspected two narrow tick windows. Not further characterized here.

**"The same record's key copied by three call sites" was wrong — they're
distinct, pass-by-value transients, not three reads of one persistent
object.** The combined address-bracket run shows a full
`sub_820D4C38`/`sub_820D4A30` construction sequence (vtable install,
zero-fill, category write) landing *between* the `lr=0x820E05D8` peek and
the `lr=0x820E778C`/`0x820E7B70` pair, at the *same* reused heap address,
with the *same* category value. Same address, same value, different
object lifetimes — the allocator handing back the identical slot for a
fresh, equal-valued transient, not one object read three times.

**Corrected demangle.** The class is
`swg::ASContext<stx::lwallocator<unsigned char,0,64>>::String` — `stx`
qualifies the allocator (not `ASContext`), `E`=`unsigned char`,
`$0A@`=`0`, `$0EA@`=`64`. `4ee47a17`'s report and the `demo-render-chain`
memory both carry the earlier, wrong grouping; fixed in this report and
in memory.

## The boxing chain: category is a plain integer argument, id is structurally always 0

`sub_820D4A30` (the class's generic-value constructor) copies a 16-byte
struct from its own second argument (`r4`, saved to `r11`) into the new
object's `+16..+31`: `[r11+0]→+16`, `[r11+4]→+20` (category, matching the
comparator's `key+4`), `[r11+8]→+24`, `[r11+12]→+28` (id, matching
`key+12`). `sub_820DA488` (confirmed vtable-dispatched, `whose_vtable.py`
resolving it as a slot-14 method on both `ASContext<...>` and
`CSwgASContext<...>` via RTTI — the live dispatch here reads a *third*,
unnamed vtable at offset 80/slot 20, plausibly the tool's own unresolved
third hit; not chased further) builds that 16-byte struct on its own
stack as `{r3, r4, 0, 0}` before calling `sub_820D4A30` — `r3`/`r4` are
`sub_820DA488`'s *own* incoming arguments. **This means: `record+16` is
the caller's own context/container pointer (unused by the comparator,
which only reads `+4`/`+12` of the copied key), category is
`sub_820DA488`'s second argument, and id is always the hardcoded `0` at
struct-offset `+8` — explaining why every `AC6_SWG_LOOKUP_KEY` line this
campaign has ever captured reports `id=0x00000000`.**

## The interpreter: a real fetch loop with a program counter and a bound

`sub_820DA488`'s one observed caller (`lr=0x82324854`, confirmed the sole
`AC6_SWG_BOX_CALL` return address across the whole run) sits inside
`sub_823246C0`, immediately after a genuine fetch-decode step:

```
r11 = [r31+20]        // this+20 -- the program counter
r10 = r11 + 4          // advance
r4  = [r11+0]          // FETCH: the word at the OLD pc -- this becomes box()'s value arg
[r31+20] = r10          // store the advanced pc back
r3  = [r31+12]          // a second field of "this" -- the context/container (matches r3 in every AC6_SWG_BOX_CALL line)
r11 = [[r3+0]+80]       // r3's own vtable, slot 20 -- resolves live to sub_820DA488
bctrl                   // box(container=r3, value=r4)
loc_82324854:
r11 = [r31+20]          // reload pc
cmplw cr6,r11,r30; blt cr6,...   // loop while pc < r30 -- r30 is the stream's own end bound
```

This is not a copy of a bounded record list — it is an ordinary
fetch/advance/compare bytecode-interpreter loop, with an explicit program
counter (`this+20`) and end pointer (`r30`). The live run confirms it:
every `AC6_SWG_BOX_CALL` at tick 3001 shares `lr=0x82324854`, and
`pc_after` (`[r31+20]` read right after the call) advances monotonically
through one contiguous buffer per interpreter instance —
`r31=0x2E3DFA08` for one (1944 box calls observed) and `r31=0x2E3BFA08`
for another (2353 calls) — both heap addresses, 0x20000 apart, matching
the campaign's established `container` pair
(`0x2E3C3D14`/`0x2E3E3D14`, `7a565550`) and its startup/title split. **The
PC steps between consecutive box calls are not a fixed 4 bytes** (28, 28,
16, 20, 24, 16, 20 bytes observed in title's own tick-3001 sequence) —
`sub_823246C0` fetches other, non-boxing words between box calls (the
float-conversion path visible at the top of its own body is one such
handler); this does **not** establish variable-length instruction
encoding, only that box() is one handler among several in a larger fetch
loop.

## The buffer's own fill: a bit-unpacking loader, on a different thread

Bracketing `[0x2DCB2000, 0x2DCB3000)` (around the observed `pc_after`
range) over the full run: the slot starts allocator-poisoned
(`0xFEFEFEFE`, thread 1, tick 39), then at **tick 2435, on thread 9 — not
the interpreter's own thread 1** — `sub_82278F78` writes it byte by byte,
each 4-byte-aligned group landing as three zero bytes followed by one
small value (`0x4D`, `0x17`, `0x2E`, `0x18`, `0x07`, `0x02`, `0x07`, ...):
exactly the shape of a compact source being *unpacked* into the
interpreter's word-per-slot runtime format, not literal per-value stores.
`sub_82278F78` itself reads several fields off an input object (`r9`,
offsets `+16/+56/+60/+80/+88/+92/+96`) through `slw`/mask arithmetic
building `(1<<n)-1`-shaped values — the structural signature of a
bitstream reader (position, buffer pointer, and bit-count fields), not a
sequence of hardcoded stores. **This is a loader, running on a background
thread, unpacking some more compact source into the buffer the
interpreter later walks — a materially different shape from `25d092bc`'s
hardcoded-construction finding**, and the first live evidence in this
campaign that swg statement data is genuinely *loaded*, not compiled in.

## What this does not establish

**Where the compact pre-unpack source comes from.** `sub_82278F78`'s own
source pointer (inside its `r9` input object) is not traced. Grepping the
flat XEX image and the store's raw `DATA00.PAC`/`DATA01.PAC`/`DATA.TBL`
bytes for the *decoded* stream values found nothing (expected: a bit-
packed source doesn't survive a byte-string search against its own
unpacked form), so this remains open, not negative evidence against the
PAC archives — a byte-level unpacking scheme has to be reversed, or the
loader's own read call traced, to settle it.

**The "M102_main_GetPlayableLevel" string, from the address-bracket run
that started this thread, is not tied to this buffer.** It is written at
tick 3001 on thread 1 (the interpreter's own thread) by `sub_82327D90`,
into a *different*, transiently-reused heap slot roughly 0x300000 away
from the bytecode buffer (`0x2E403D50` vs `0x2DCB2xxx`) — not shown to be
sourced from the same loader. `grep -abo`/`strings` found it nowhere in
the flat XEX image or the raw (uncompressed-view) PAC/TBL bytes either.
Given category `0x17` (SendMsgI, whose tag decodes to `"M102"`,
`AC6_DEMO_M102_RESOLVES_TO_A_QUERY_NOBODY_CURRENTLY_ANSWERS`) is in the
same tick-3001 batch, this is plausibly the query's own full name,
assembled at runtime from pieces — but `sub_82327D90` is unread, so this
is a lead, not a finding.

- `sub_823246C0`'s other fetch handlers (the float-conversion path, and
  whatever handles the non-box() words in the 16-28-byte gaps between
  box calls) — unread.
- The heartbeat instance's own buffer (the `r31` not seen in title's
  batch) — not separately bracketed.
- The third, RTTI-unnamed vtable `whose_vtable.py` found for
  `0x820DA488` (slot 20, matching the live dispatch) versus the two named
  hits (slot 14) — not reconciled; plausibly a different concrete class
  in the same family, not chased.

## Gates

Source changed: `swg_native_call_trace.hpp` gained one opt-in trace block
(`AC6_DEMO_WATCH_SWG_BOX_CALL`), reusing the existing dispatch path, no
behavior change when unset. Both build trees rebuilt. Native gate JF,
demo `ctest`, and both contract audits verified below before commit.
