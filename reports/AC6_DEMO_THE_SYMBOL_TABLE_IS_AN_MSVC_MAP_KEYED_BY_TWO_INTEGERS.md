# The symbol table is an MSVC `std::map`, keyed by two integers, not a name string

## Qualification

AC6 demo PAL, same XEX SHA-256. Static re-read of generated code already in
the tree: `sub_820D5470` (`ppc_recomp.5.cpp:3081-3135`) and `sub_820D4748`
(`ppc_recomp.5.cpp:1068-1143`), the two functions
`AC6_DEMO_THE_MARSHALLERS_ONE_CALLER_IS_A_TYPED_AST_NODE_EVALUATOR.md`
(`f7c4e68f`) left unread inside `sub_820DFD28`'s lookup. No new run.

## Correcting `f7c4e68f`

That report described the `twi 31,r0,22` sanity checks in `sub_820DFFB8`
as triggering "if `[r1+80]==0` or `[r1+80]==context+36` itself" — backwards.
Reading the control flow precisely: `r11==0` falls straight into the trap
(no branch skips it); `r11==r30` branches *around* the trap to
`loc_820E0008`; the only path that reaches `loc_820E0004` (the trap) at all
is falling through from the second `beq` not being taken, i.e. `r11 != 0
&& r11 != r30`. **The assert requires `[r1+80] == context+36` (`r30`) and
traps otherwise** — the same "found the polarity backwards" failure class
`ee81086d` corrected for `GetCurrentMission` two commits ago, caught here
before it reached a downstream claim. `sub_820DFD28` has the identical
shape (`r11==0` or `r11!=r31` traps, `r11==r31` — the lookup root argument
there — is required) and was already read correctly in that report's prose,
so only `sub_820DFFB8`'s description needed fixing.

## `sub_820D5470`: an MSVC `std::map` lookup helper

`sub_820D5470(r3=out[2], r4=root, r5=key)`: calls
`sub_820D4DB8(root, key)` (unread — the actual tree descent), gets a node
pointer back in `r3`, then builds a **checked iterator**: `out[4]=r3` (the
node), asserts `root != 0`, `out[0]=root` (the container). This is the
canonical MSVC `xtree` `lower_bound`/`find`-family shape: a debug-checked
iterator is exactly `{container_ptr, node_ptr}`, and the container field
gets written unconditionally to `root` regardless of what the tree walk
found. `sub_820DFD28`'s own fallback path (`{root, [root+4]}` when the
found node's second word equals `[root+4]`) is simply **`end()`** — `[root+4]`
is the map's own end/head sentinel node, not a "scope fallback" as
`f7c4e68f`'s prose speculated; there is no scope-chain semantic here, just
ordinary "not found."

## `sub_820D4748`: the comparator, and it is not a string compare

`sub_820D4748(r3=lhs_key, r4=rhs_key)` reads exactly two integer fields
from each side — `[key+4]` and `[key+12]` — and touches no byte/char data
anywhere in its body. Structure:

- `[lhs+4] == -1` or `[rhs+4] == -1` is treated as a wildcard/boundary
  case (several branches short-circuit around it, converging on
  `[rhs+4]==-1` implying "lhs is not less than rhs" — `return 0`).
- Otherwise, `[lhs+4]` vs `[rhs+4]` decide directly: return `1` (true) if
  `lhs+4 < rhs+4`, `0` if greater; only on **equality** does it fall
  through to compare the second field.
- The second field, `[lhs+12]` vs `[rhs+12]`, decided via the
  `subfc`/`subfe` borrow-bit idiom — the standard PPC pattern for
  `(uint32_t)lhs < (uint32_t)rhs` — and that bit is the function's return
  value.

This is a strict-weak-order `bool operator<(const K&, const K&)` over a
**two-field integer composite key** — `(category, id)`, `category`
sorting first and wildcard-capable (`-1` = "any"), `id` breaking ties,
both unsigned 32-bit. `sub_820DFD28` calls it as
`sub_820D4748(search_key, found_node + 12)` — so the stored key inside
each tree node begins at `node+12`, and the comparator's own `+4`/`+12`
offsets are relative to *that* base: node-relative, the compared fields
sit at `node+16` (category) and `node+24` (id).

**The standing "read the bytecode as a name string" framing does not fit
this evidence.** Nothing this campaign has traced through the swg
subsystem — `SendMsgI`'s tag decoded to the integer `102`
(`883d396d`), the AST-node evaluator's `type==2` check (`f7c4e68f`), and
now this comparator's pure-integer key — touches a string. The symbol
table this evaluator consults is keyed by small integer pairs, not names;
"script vocabulary" in this codebase reads as closer to a fixed enum space
than to identifiers.

## Reading

The chain end to end, corrected and completed: AST-node evaluator
(`sub_820DFFB8`) → `sub_820DFD28` (map lookup wrapper) → `sub_820D4DB8`
(tree descent, unread) + `sub_820D5470` (checked-iterator construction) +
`sub_820D4748` (the `(category, id)` comparator) → back in
`sub_820DFFB8`, a type check on the found node's own `+16` field (the
node's stored *value*, not the key just used to find it — offset `28` off
the node per `f7c4e68f`'s earlier read, `[[r10+28]+4]==2`) gates the
native-call dispatch.

## Not established

- `sub_820D4DB8`'s own body (the tree descent) — inferred from its
  call/return shape and `sub_820D5470`'s use of the result, not read.
- What the search key struct's own `+0`/`+8` fields hold, or how large it
  is — only `+4` (category) and `+12` (id) are ever read by the
  comparator.
- What concrete `(category, id)` values the swg evaluator actually looks
  up, live — this report is static; the next step is logging them.
- Whether every node's value at `+16..+19` uses the same `type==2` = native
  call convention, or whether other type values map to other evaluator
  behaviors not yet found.

## Gates

No source changed; report-only commit.
