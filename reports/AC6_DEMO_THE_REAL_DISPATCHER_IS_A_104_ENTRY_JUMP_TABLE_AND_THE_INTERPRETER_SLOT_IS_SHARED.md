# The real bytecode dispatcher is a 104-entry jump table; `sub_823246C0` is one opcode's handler, and the tracked heap slot is shared by 26+ owners

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: the existing
`AC6_DEMO_WATCH_ADDR_LO/HI` bracket instrument (no source change), widened
to `[0x2E3DFA08, 0x2E3DFA20)` — the full 24-byte header of the heap object
this campaign has called "title's interpreter instance" since `b67e7f6f` —
run with `probe --until frontend --max-ticks 3200`, correctly-timed START,
no oracle. Static: full reads of `sub_82325160`
(`ppc_recomp.44.cpp:1010-1058`) and `sub_820AC748`
(`ppc_recomp.1.cpp`), and a raw word dump of `.build/Default.xex.base.bin`
at `0x8264CEA8`.

## What this corrects

**A pending validation gap in `6d61b5cd`.** That report said the shared
RTTI `pvftable` was "validated against 771 other occurrences... all
followed by a proper `.?AV` name," but the check behind that sentence only
inspected the first 10 of 772 hits, and used a too-narrow prefix
(`.?AV`, the MSVC tag for *class* specifically). A full pass over all 772
hits, accepting any valid MSVC RTTI tag (`.?A` — covers class/struct/union/
enum), resolves **769 of 772** to a readable name; the remaining 3 read as
`None` (short reads near the image's tail, not investigated further). The
undercount was in the checking script, not the underlying claim — `6d61b5cd`'s
conclusion (the shared vtable reliably marks RTTI type descriptors) stands,
now to a number that was actually computed in full.

**`sub_823246C0` is not "the interpreter."** Every report since `346255b2`
has called it that — a fetch/advance/box loop with an explicit PC at
`this+20`. It is real, and everything said about it is still true, but it
is not the top-level dispatcher: it is **the handler for opcode value
`0x2E`** inside a larger jump table this campaign had not yet found. Two
independent lines confirm this:

- **Static**: a raw dump of the table at `0x8264CEA8` (`0x8264CDF8`'s own
  table plus its RTTI descriptor sit immediately before it — the same
  neighbourhood `6d61b5cd` already read) shows `index 0x2E =
  0x823246C0` exactly.
- **Live**: in this run, `sub_823246C0` is entered with `lr=0x823251B8`
  (`stderr.log`, e.g. tick 2452) — and `0x823251B8` is `sub_82325160`'s
  own `bctrl` return address (`ppc_recomp.44.cpp:1047`), not a `bl` from
  anywhere else. The dispatch is live-confirmed, not just table-shaped.

**Category `0xB`'s "not found" result (`4ee47a17`, the campaign's very
first finding in this arc) now has a structural cause.** Table index
`0xB`'s handler is `0x820AC748` — read in full, it is one instruction:

```
PPC_FUNC_IMPL(__imp__sub_820AC748) {
  PPC_FUNC_PROLOGUE();
  return;   // blr, nothing else
}
```

A pure `blr`. Opcode `0xB` is not a data-dependent lookup miss; it is an
**unimplemented opcode**, wired to a no-op stub. The same stub fills at
least ten other slots in the table (indices `0xB, 0xF, 0x10, 0x14, 0x1C,
0x1D, 0x1E, 0x23, 0x24, 0x26, 0x28`, from a 0-103 dump) — `0xB` was never
special, it just happened to be the one this campaign's script tried to
evaluate.

**`390cfe33`'s "one confirmed yield/resume" is strengthened, not
undermined, by a check this report ran precisely because the next finding
made it necessary (see below).** Comparing the *owner* field (this
report's own new find) at tick 3001 and tick 3033 — not just the PC field
`390cfe33` used — both read `0x2E3EDA90`. The two ticks are the same
owner resuming, confirmed on a second, independent field.

## The heap slot is a reused execution-context object, not "title's own interpreter"

The widened bracket's first new fact: `[0x2E3DFA08, 0x2E3DFA20)` is
poisoned wholesale (`0xFEFEFEFE`, all six words) at **tick 40**, by
`sub_823273E0` — a pool allocator's debug fill pattern, consistent with
every other heap-range signature this campaign has seen. At **tick 2451**,
`sub_82324188` (unread before this report) writes `+0` (a vtable pointer,
`0x2E3E3AD4`) and `+12` (`0x2E3E3D14`, unidentified) — the constructor.
Dispatch begins the very next tick.

From tick 2452 on, every dispatch writes two more fields via the already-
traced thunk chain: `sub_82325288` (`390cfe33`'s "thunk") writes `+4` (an
**owner** back-pointer, its own `r5` argument) and `+8` (a reset flag,
always `0`); `sub_823251E0` writes `+16` (**`table_base`**) and `+20`
(the initial PC, `table_base + offset`).

Across the whole 3200-tick run, **`+4` (the owner) takes at least 26
distinct values** (`0x2E3EDA90, 0x2E3F1350, 0x2E3F2650, ... 0x2E40F250`,
all in the same heap-allocator range as everything else in this
campaign), while **`+16` (`table_base`) takes exactly one value the whole
run: `0x2DCB1220`.** This is the report's central correction to the whole
`b67e7f6f`/`390cfe33`/`6d61b5cd` arc: `0x2E3DFA08` is not a persistent
"title interpreter" — it is a **reusable execution-context slot**, handed
out to (at least) 26 different owning instances in sequence, each running
the *same* compiled program from a different starting offset. What those
three reports actually observed — the 38-value entry table, the per-tick
re-entrant loop, the yield/resume — is real, but it is the activity of
many owners funneling through one recycled slot, not one continuous
"title" execution. The dominant idle-check owner (`0x2E4035D0`, stable for
all 429 consecutive dispatches from tick 2572–3000, matching `b67e7f6f`'s
count exactly) and the press/resume owner (`0x2E3EDA90`, dispatched only
at ticks 2452, 2571, 3001, 3033) are two *different* instances of
whatever class owns this slot, both running the one shared program.

**`table_base = 0x2DCB1220`, not `0x2DCB2000`.** `1fcc88b3` reconstructed
a 4KB window `[0x2DCB2000, 0x2DCB3000)` from write-log addresses alone and
validated it 10/10 against a live capture — that reconstruction is not
wrong, but it described a sub-window starting `0xDE0` (3552) bytes into
the real buffer, never having read `table_base` directly. This run's own
PC-field writes for owner `0x2E3EDA90`'s first dispatch (`0x2DCB23A4,
0x2DCB23A8, ...`) land inside `1fcc88b3`'s window, confirming it is a
correctly-read slice of the same underlying buffer, just not the buffer's
own start.

## `sub_82325160`: the real outer loop, read in full

```
r31 = r3                      // the execution-context object
if [r31+20] == 0: return      // empty PC -- nothing queued, done
r30 = 0x8264CEA8               // the jump table base (computed via lis/addi)
loop:
  r11 = [r31+20]               // fetch PC
  r10 = [r11+0]                 // fetch the word at PC
  [r31+20] = r11 + 4             // advance PC
  if r10 == 0: return            // word 0 terminates this run
  r11 = table[r30 + r10*4]        // opcode dispatch
  call r11(r31)                    // handler runs, may advance/reset PC itself
  if [r31+20] != 0: goto loop        // still runnable -- keep going
```

This settles what "opcode `0`" means in the bytecode template
`1fcc88b3` validated (`[0x16, 0x19, 0x2E, 0x08, 0x00, category]`): it is
not a no-op instruction inside `sub_823246C0`'s own inner loop as this
campaign had implicitly assumed — it is this outer loop's **terminator
word**, consumed by `sub_823246C0` itself (opcode `0x2E`'s handler) when
`0x2E`'s own inner fetch/box loop runs past it, one level down. Nothing
in this report re-validates that finer point live; it is a reading of the
two loops together, not a new capture.

## Not established

- The class/identity of any of the 26+ owner objects (`+4` values) — not
  read. Naming `0x2E3EDA90` (the press/resume owner, dispatched only four
  times in 3200 ticks) versus `0x2E4035D0` (the dominant per-tick owner)
  is the natural next step, same shape as this campaign's own repeated
  "read the vtable, get the RTTI name" method.
- The constructor's `+12` field — written once, at construction, never
  observed to change; not read.
- Whether the shared program at `table_base=0x2DCB1220` is one compiled
  resource read by all 26+ owners, or `table_base` simply hasn't varied
  within this particular 3200-tick sample — only one value has ever been
  observed, from one run.
- Whether EndMode's own statement (`0x2DCB2024`-`0x2DCB2038`, `1fcc88b3`)
  is ever the *target* of a dispatch by any of the 26+ owners specifically
  — `1fcc88b3`/`b67e7f6f`'s "never fetched, by address, across the whole
  run" finding is unaffected by anything in this report (it never assumed
  a single owner), but it has not been re-cross-checked against the owner
  field either.
- The table's own extent past index ~103 (`0x8264D044` breaks the
  code-pointer pattern; `0x8264D048`-`0x8264D054` read as plain small
  integers `0,1,2,3`, unrelated data) — not chased further.

## Gates

No source changed; this report uses only the pre-existing
`AC6_DEMO_WATCH_ADDR_LO/HI` instrument with a wider bracket, plus static
reads. Native gate JF, demo `ctest` (26/26), and both contract audits
verified below before commit.
