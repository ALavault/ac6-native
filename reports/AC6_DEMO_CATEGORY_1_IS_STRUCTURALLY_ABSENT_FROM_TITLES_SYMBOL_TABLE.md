# Category 1 is structurally absent from title's own symbol table — confirmed by direct traversal, not runtime non-evaluation

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: `probe --until frontend
--max-ticks 3200`, headless backend, no oracle, correctly-timed START. New
read-only instrument reimplementing the guest's own map lookup
(`find_swg_symbol_node`, `resolve_swg_symbol_type_tag`,
`dump_swg_symbol_table` in `guest_bridge/swg_native_call_trace.hpp`) to
walk the symbol table directly from guest memory, independent of what the
guest happens to evaluate at runtime.

## What this closes

`AC6_DEMO_CATEGORY_IS_THE_NATIVE_FUNCTION_ID_TITLE_NEVER_EVALUATES_CATEGORY_1.md`
(`33b549ef`) established what title's script *evaluates* but explicitly
could not distinguish "category 1 absent from the script" from "present
but unreached" — the instrument only saw lookups the guest actually
performed. This report reimplements the lookup itself (confirmed correct
by direct read of `sub_820D4DB8`/`sub_820D4748`/`sub_820DFD28`,
`a015f994`/`f7c4e68f`) and an in-order tree traversal, so it can answer
"is category 1 registered at all" independent of runtime evaluation.

## Two bugs in the first draft, caught before commit

Building the reimplementation and cross-checking it against known-good
live results (the five confirmed `type==2` native calls at tick 3001)
caught two errors in the instrument itself before any of this was
committed:

1. **Comparator argument order reversed in the tree descent.** `sub_820D4DB8`
   calls the comparator as `comparator(node_key, search_key)` — node first
   — not `comparator(search_key, node_key)` as first implemented. Getting
   this backwards makes every descent go the wrong direction; the first
   version returned "not found" for every single lookup, including ones
   the guest itself resolved and called natively moments earlier in the
   same trace. (The final equality check in `sub_820DFD28`, which *is*
   `comparator(search_key, found_key)` — search first — was correct from
   the start; only the descent's argument order was backwards.)
2. **Type tag offset collapsed a pointer hop.** `f7c4e68f`'s own read
   established the check as `[[node+28]+4] == 2` — `node+28` holds a
   *value pointer* (the pair's `mapped_type`, stored right after the
   16-byte key struct starting at `node+12`), and the type tag is 4 bytes
   into *that* object, not `node+32`. The first draft added the offsets
   together instead of dereferencing twice, producing plausible-looking
   but meaningless values (`0x01000000`, `0x00000000`) for every entry.

Both are fixed (`less()` calls swapped to node-first inside the descent
loop only; a `resolve_swg_symbol_type_tag` helper added, doing the
pointer hop explicitly) and verified: all five categories that the live
trace already confirmed produce a native call (`4`, `5`, `6`, `0x17`,
`0x13`) now resolve, via this independent reimplementation, to the exact
node addresses the structural dump also finds, each reporting
`type_tag=0x00000002` — matching `f7c4e68f`'s `cmpwi cr6,r9,2` exactly, by
an entirely separate computation path. Category `0x0B` (11) — the one
lookup in the batch that never produced a call — resolves to `node=0,
type_tag=0`: **a plain not-found lookup, not a node of some other,
uncharacterized type.** This retracts `33b549ef`'s framing of category 11
as "the highest-value unidentified target" — it isn't a mystery kind of
statement, it's an ordinary failed lookup, structurally identical to
category 1's fate, the only difference being that the script attempts to
look it up at all.

## The decisive result: title's registered table, read directly

`dump_swg_symbol_table`, run once per distinct context, in-order traversal
using the tree's own `_Left`/`_Right` pointers (no comparator involved —
correct regardless of the lookup bugs above). Title's context
(`0x2E3EAA94` this run) registers exactly **nine** concrete-category
entries, all `type_tag=2` (native-call bindings):

```
category=3   node=0x2E3ED210
category=4   node=0x2E3ED390   (GetCurrentLevel)
category=5   node=0x2E3ED310   (GetCurrentMission)
category=6   node=0x2E3ED290   (GetCurrentMode)
category=0x13 (19) node=0x2E3EC590   (NUD_TONE_BANK)
category=0x17 (23) node=0x2E3EB790   (SendMsgI)
category=0x18 (24) node=0x2E3EB110
category=0x19 (25) node=0x2E3EC410
category=0x1A (26) node=0x2E3ECA10
```

Plus 33 `category=0xFFFFFFFF` (wildcard) entries whose `id` field is a
large pointer-shaped value, not a small integer — evidently a different
kind of binding (per-instance object references, not native-function
IDs), out of scope here.

**Category `1` — the completion trigger's own ID — does not appear
anywhere in this list.** Nine entries were read in full; none is `1`.
This is a direct structural fact about the registered table, obtained
without evaluating anything and without relying on the (now-verified, but
originally buggy) lookup algorithm at all. **The (a)/(b) split
`33b549ef` left open is closed: category 1 is genuinely absent from
title's script's own symbol table, not merely present-but-unreached.**
Title's script object is never bound to the completion function at all —
this is a registration-time fact about the script instance, not a
runtime control-flow outcome three separate forcing experiments already
showed nothing could redirect.

## An open discrepancy, noted and set aside

A second, unrelated context (`0x2E3E7C94` this run — not title's) drives
the same per-tick "heartbeat" AST node (`category=0x2F`/47) continuously,
every tick, both before and after the press (`33b549ef`). This
reimplementation resolves that lookup to a real node with `type_tag=2` —
which by the same logic that correctly predicts all five of title's native
calls, should produce a marshaller call every tick. It never does, in any
run this campaign has traced. This is a genuine, reproducible mismatch
between the reimplementation and the guest's real behavior for this one
context/category, not yet explained (candidates: an additional gate this
report's static reading of `sub_820DFFB8` didn't fully capture, or a
subtlety in the wildcard-heavy tree structure this context's table has —
34 of its ~36 entries are wildcard-keyed, unlike title's all-concrete
table). It does not touch this report's title-specific conclusion, which
rests on the structural dump (comparator-independent) for the closing
claim and on five independently cross-validated matches for the lookup
algorithm's correctness where it matters.

## Not established

- What categories `3`, `0x18` (24), `0x1A` (26) — registered in title's
  table but never observed evaluated live — actually are. Concrete next
  step: cross-reference their `node+28` value pointers against the static
  command table the same way `4`/`5`/`6`/`0x13`/`0x17` were already
  identified by address.
- The heartbeat/`0x2E3E7C94` discrepancy above.
- Whether category 1 is absent from *every* title-context instance across
  runs, or specific to this one (stack addresses vary run to run, but the
  registration content should not — not re-verified on a second run).
- What registers these entries in the first place — the map's *insertion*
  site remains unread; this report answers "what's registered" by reading
  the finished tree, not by finding who builds it.

## Gates

New helper functions (`find_swg_symbol_node`, `resolve_swg_symbol_type_tag`,
`dump_swg_symbol_table`) and a new opt-in env var
(`AC6_DEMO_DUMP_SWG_SYMBOL_TABLE`), all read-only, all in the header — no
new line in `AC6_PPC_CALL_INDIRECT`. Native gate JF, demo `ctest` (26/26),
and both contract audits verified below before commit.
