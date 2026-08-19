# `sub_820D5B90` is a list-non-empty assertion, not a bespoke gate

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Static read only:
`recompilation/ace-combat-6-demo/build-codegen-on/codegen/generated/
ppc_recomp.5.cpp:4189-4233` (`sub_820D5B90`, its complete body — 44
generated lines, the whole function) and `:4236-4340+`
(`sub_820D5BE0`, its sibling/caller, read through its first three guard
checks). No probe run, no source change.

## What this closes

`1b87123e` found three independent call sites (`sub_820DBA18`,
`sub_820D7700`, `sub_820E7638`) all reaching the same shared helper,
`sub_820D5B90`, and all failing whatever it checks — but left "what
`sub_820D5B90` is FOR" explicitly unresolved: "never named, never
connected to a specific engine subsystem... by anything stronger than its
own instruction shape." This report reads the function in full for the
first time.

## The whole function, decoded

`sub_820D5B90(this)`:

```
r10 = [this+4]              -- a node pointer stored at this+4
r11 = [r10+4]                -- that node's OWN +4 field
if (r11 == r10) trap         -- self-referential -> abort
else call sub_820D5878(&this_and_r11_packed) and return
```

**This is exactly a "the list is not empty" assertion**, implemented via
the standard intrusive-sentinel idiom this campaign has already
encountered elsewhere in this codebase (the `swg` symbol table's own MSVC
`xtree` node layout, `find_swg_symbol_node`'s header comment): a freshly
constructed or fully-drained list's head sentinel points to itself.
`sub_820D5B90` traps precisely when the node at `[this+4]` is in that
self-pointing state — i.e., precisely the state **every** instance of
these fields this campaign has ever observed live (`29da1b05`: "`[+4]` is
observed only in two states: self-pointing... or explicitly zeroed... No
run in this campaign has ever observed `[+4]` holding a genuinely
different, non-self, non-zero pointer").

`sub_820D5BE0` (the function `1b87123e` found calling into `sub_820D5B90`
via `r27`) is a larger, sibling routine with the **same shape three
times over**: `cmplw`/`cmplwi` consistency checks between a node's stored
`prev`/`next`-shaped fields (`r8`/`r9`/`r10`/`r11`/`r28`/`r29`, all loaded
from a caller-supplied argument block), each followed immediately by
`twi 31,r0,22` (an unconditional trap instruction) on the failing branch,
before finally calling `sub_820CFC58` (already known to this campaign as
a node-release/pop helper, `29da1b05`) and writing a new node's pointers
into the caller's own storage (`[r30+0]`/`[r30+4]`). This is the
insertion half of the same container: verify the node about to be
spliced in is mutually consistent with its neighbors, then link it in.

## Reading: generic container plumbing, not a bespoke mission-manager gate

The combination — a self-reference-implies-empty check, paired with an
insertion routine carrying multiple prev/next consistency traps — is the
standard shape of a debug-mode intrusive doubly-linked list (the same
family of defensive checks MSVC's checked/debug STL containers emit),
not game-specific validation written for one particular subsystem. This
sharpens, without contradicting, `1b87123e`'s closing line ("consistent
with... the mission-scoped `CX360UnitManager` explanation"): the check
itself is generic list machinery, reused by however many lists in the
engine need it — the *reason* title's specific list is always empty is a
question about what code is supposed to insert into *this* list, not
about what `sub_820D5B90` uniquely gates. That question remains
unanswered: no code path inserting into any of these four `ASContext`
lists (`+292`/`+324`/`+336`/`+356`) has ever been observed executing, on
any route this campaign has tried.

## Not established

- The exact address-0 mechanics of the live trap `1b87123e` observed
  (`lr=0x820E7840`, "unmapped 32-bit read, address=0") — this report
  explains the *check*'s logic but did not step the specific failing
  call instruction-by-instruction to confirm which dereference reads
  literal address `0`; plausibly the caller's own `this` argument (`r27`)
  is itself null in that instance, one level earlier than the check this
  report reads, but that is not confirmed here.
- What code, anywhere in the atlas, is meant to call `sub_820D5BE0` (or an
  equivalent insert routine) to populate one of these lists — not
  searched for in this report.
- Whether this generic-container reading is correct beyond instruction
  shape (e.g., an actual MSVC debug-STL signature match, per this
  campaign's `name_xdk_library_function.py` method) — not attempted;
  this report's claim rests on shape alone, stated as a reading, not a
  confirmed identification.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. No source change — pure static read.
