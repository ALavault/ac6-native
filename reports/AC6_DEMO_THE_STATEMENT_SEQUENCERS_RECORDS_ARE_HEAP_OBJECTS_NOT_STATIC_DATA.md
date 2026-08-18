# The statement sequencer's records are heap-allocated `swg::stx::ASContext<...>::String` objects, not static XEX data

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: a new opt-in trace,
`AC6_DEMO_WATCH_SWG_RECORD_KEY_CALL`, gated on `guest_address==0x820DEDF8U`
inside the existing `AC6_PPC_CALL_INDIRECT`-fed `trace_swg_native_call`,
logging `lr`, `r3`, `r4`, `r5` on every call — run together with the
existing `AC6_DEMO_WATCH_SWG_LOOKUP_KEY` trace, `probe --until frontend
--max-ticks 3200`, correctly-timed START, no oracle. Static: a raw
data-word scan of `.build/Default.xex.base.bin` for `0x820DEDF8`
(`sub_820DEDF8`'s own address) plus `tools/whose_vtable.py` against it.

## What this corrects

The prior draft of this report (uncommitted, same investigation) read
`sub_820DEDF8`'s body and its one observed caller inside `sub_820E7638`,
then concluded — on that static reading alone, before running the one
experiment that could tell static image data from a runtime heap object
apart — that this was "the compiled statement data this campaign has
hunted for since early in the campaign." Advisor review caught this before
commit: the record-copy mechanism was solid, but the record *source* was
an open disjunction (static image address vs. heap address vs. a reused
cursor), and the title asserted the strong branch of that disjunction
without having run the discriminator. This report runs it.

## The instrument, and the result

`trace_swg_native_call` already receives `guest_address` (the resolved
`bctr` target) on every indirect call in the guest, independent of `lr` —
the same path that already named `sub_820E8F90`'s callers
(`AC6_DEMO_WATCH_SWG_VM_LOOP_CALL`). Adding one more gate,
`guest_address==0x820DEDF8U`, logging `r4` (the "source record" argument)
needed no new dispatch plumbing.

Across the whole 3200-tick run, **every single `r4` value observed is in
the `0x2Exxxxxx` range** — the same heap-allocator range this campaign has
seen node addresses in throughout the symbol-table work (`a015f994`,
`7a565550`). **Not one is a static image address** (`0x82xxxxxx`). The
records `sub_820DEDF8` copies from are runtime-constructed heap objects.

Zooming into title's tick-3001 batch, chronological interleaving of the
two traces (`stderr.log:3722-3754`) pins, for the first time, the actual
evaluation order and gives a clean one-to-one pairing between each
`lr=0x820E7B70` call and the `AC6_SWG_LOOKUP_KEY` line immediately after
it:

| record `r4` | category | lookup result |
|---|---|---|
| `0x2E3F1B54` | `4` | found, `node=0x2E3ED390` |
| `0x2E403914` | `5` | found, `node=0x2E3ED310` |
| `0x2E403994` | `6` | found, `node=0x2E3ED290` |
| `0x2E403B54` | `0x17` | found, `node=0x2E3EB790` |
| `0x2E403D54` | `0xB` | **not found**, `node=0` |
| `0x2E3F1714` | `0x13` | found, `node=0x2E3EC590` |

All **six record addresses are distinct** — resolving the prior draft's
open "single mutable cursor vs. six distinct objects" question in favor of
six distinct objects, in this fixed order. `33b549ef` already established
title never evaluates category 1; this table adds that the one category
in its batch that fails to resolve at all is `0xB`, and names the exact
record object (`0x2E403D54`) whose key that failure traces to.

## The class: `swg::stx::ASContext<lwallocator<char,0,10>>::String`

A raw scan of the flat image for the literal word `0x820DEDF8` finds
exactly two hits outside `.pdata`. `tools/whose_vtable.py` resolves one of
them cleanly via RTTI:

```
0x820DEDF8 appears in 2 aligned word(s) outside .pdata: 1 named, 1 unnamed
    at 0x82006B94   vtable 0x82006B44 slot +0x50   .?AVString@?$ASContext@V?$lwallocator@E$0A@$0EA@@stx@@@swg@@  [RTTI]
```

Slot `+0x50` is slot 20 (80/4) — the exact slot `sub_820E7638`'s dispatch
reads. This names the concrete class whose virtual "get my key" override
is `sub_820DEDF8`: `swg::stx::ASContext<lwallocator<char, 0, 10>>::String`
(MSVC mangling, read informally — not run through a full demangler). The
`swg` namespace matches every other piece of this subsystem traced so far
(`SendMsgI`, `GetCurrentMission`, the symbol table); `ASContext` reads as
"ActionScript Context," consistent with this campaign's standing
identification of `sub_820E8F90` as the ActionScript/swg native-call
marshaller. The second hit (`0x82079280`) resolves to no RTTI locator
within the tool's search span — a second class (or the same class's other
vtable, e.g. for a different allocator instantiation) that this report
does not further identify.

A named class does not by itself decide static-vs-heap — a vtable pointer
is always a static image address regardless of where an *instance* lives.
It is the `r4` log above, not this scan, that establishes the instances
are heap objects. What the class name adds is a name for the constructor
hunt this finding now motivates (see Not established).

## `sub_820DEDF8` itself: a plain 16-byte struct copy, unaffected by this correction

Static: `sub_820DEDF8`'s full body (`ppc_recomp.5.cpp:25915-25978`).
`(r3=destination, r4=source_record, r5=<unused in the literal path>)`:

```
if [source_record+12] != 0:
    call sub_820DC768(destination, source_record+32, ...)   // computed path, unread
else:
    destination[0..3]  = source_record[16..19]
    destination[4..7]  = source_record[20..23]   // category
    destination[8..11] = source_record[24..27]
    destination[12..15]= source_record[28..31]   // id
```

Category and id land at `destination+4`/`destination+12` — matching the
comparator's own `key+4`/`key+12` reads (`a015f994`) — so within the
*source record*, category is at `+20` and id is at `+28`, not `+16`/`+24`
as the map node's own key-embedding offset would suggest. The live
capture pins this exactly: bracketing `[0x7F04054C, 0x7F040550)` —
`destination+4` only, via the pre-existing `AC6_DEMO_WATCH_ADDR_LO/HI`
instrument, in an earlier run of this same investigation — showed that
address's value matching the reported category in all six of title's
tick-3001 writes. Byte `+12` of the *source record* (a different, outer
record from the key struct itself) is a type flag: `0` means "the key is
a literal, stored right here"; nonzero routes to `sub_820DC768` with
`source_record+32` instead — unread, presumably for keys that must be
computed rather than copied verbatim.

`sub_820DEDF8` has no direct `bl` caller anywhere in the image — every
call reaches it through some vtable's slot 20. The specific dispatch this
campaign has followed sits inside `sub_820E7638`
(`ppc_recomp.6.cpp:16916-16925`):

```
r11 = [r24 + 0]        // r24's own vtable
r11 = [r11 + 80]       // slot 20 (80/4)
mtctr r11; bctrl        // args: r3=dest(r1+128), r4=r24, r5=r25
```

`r24` is passed as `sub_820DEDF8`'s own `r4` — the "source record" the
copy reads from. `r25`, address-adjacent to the loop's own locals and
carrying a reference count at `[r25+272+4]` incremented just before the
call, is consistent with being the list/container `r24` is walked from,
but this rests on one refcount line, not a confirmed container read.

## Not established

- **The constructor.** `25d092bc` traced native-function *symbol*
  registration to hardcoded C++ construction, with a fixed, small call
  chain up to a shared allocator (`sub_820CE750`). Whether these
  *statement* records are built the same way — a small, enumerable set of
  constructor call sites, the shape this report's own class name now makes
  searchable via `swg::stx::ASContext<...>::String`'s own constructor(s) —
  is the direct next move, not yet taken.
- **Three call sites per record, not one.** Within a single tick-3001
  iteration, the *same* record's key was copied into three different
  destinations by three different call sites in sequence:
  `lr=0x820E05D8` (dest `0x7F040768`), then `lr=0x820E778C` (dest
  `0x7F040548`), then `lr=0x820E7B70` (dest `0x7F040548` again, the one
  this campaign has been tracking). None of the three intervening
  functions (`sub_820E05D8`, `sub_820E778C`) has been read. The prior
  draft's "the one call site" framing undercounted even the calls
  reaching a single record, not just the population of records.
- **A fourth, distinct call site with a stable address that turns out to
  be the sixth record.** `lr=0x820D7818` (dest `0x7F040748`) fires
  repeatedly through the batch with one constant `r4=0x2E3F1714` — and
  that exact address is also the sixth and last record's own `r4` in the
  table above. Whether this is a look-ahead/sentinel pattern (a
  next-node or end-of-list check performed once per iteration against the
  same object that eventually becomes "current") or something else is not
  read. `sub_820D7818` itself has not been examined.
- `sub_820DC768`, the computed-key path (reached when a record's own
  `+12` flag is nonzero) — still unread.
- The second, unnamed vtable hit (`0x82079280`) — not pursued.

## Gates

Source changed: `swg_native_call_trace.hpp` gained one new opt-in,
env-var-gated trace block (`AC6_DEMO_WATCH_SWG_RECORD_KEY_CALL`), reusing
the existing `AC6_PPC_CALL_INDIRECT` dispatch path, no behavior change
when unset. Both build trees rebuilt (`build` for the gate suite,
`build-codegen-on` for the probe run). Native gate JF, demo `ctest`, and
both contract audits verified below before commit.
