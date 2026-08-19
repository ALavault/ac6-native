# The dominant owner survives the press; its enqueue call site simply
# stops firing

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. One live probe run, fully natural: `--until
frontend --max-ticks 3600 --input-at 3000,16,0,0,0,0,0,0,1 --input-at
3001,0,0,0,0,0,0,0,1` (`634cff33`'s tuple), neutral store,
`AC6_DEMO_WATCH_ADDR_LO/HI` bracketing `[0x2E4035D0, 0x2E403830)` — the
dominant owner's own object header (`0x2E4035D0`, `a42271ec`), not its
array (`0x2E403994`, already read by `74756ffc`). No source change.

## What this closes

`74756ffc` found the dominant owner's `MovieMemory` array reallocated to
unrelated objects right at the press, and left open whether "the owner
itself is destroyed, or only this array is reallocated while the owner
persists." This report checks the owner's own object header directly.

## The owner is not destroyed — its vtable is stable across the press

`[0x2E4035D0]` (the owner's own vtable pointer) is written four times in
the whole 3600-tick run: tick 40 (pool-poison, pre-construction), tick
2571, and three times at tick 3001. **Every one of these writes, except
one, is the identical value `0x820304D8`** — the same class vtable before
and after the press. The one different value, `0x82006544` at tick 3001
(`lr=0x82323728`, sandwiched between two `0x820304D8` writes at the same
tick), is a transient — the object is back to its normal vtable by the
end of the same tick, not left in a different class's state the way
`74756ffc` found the array region. **The dominant owner's object is not
destroyed or reused at the press.**

The broader bracketed region (`+0x00` to `+0x260`) stays intensely active
the entire run, through the last sampled tick (`3599`): 8405 writes
sampled in the `[3010,3200]` window alone, from more than two dozen
distinct writer functions (`sub_82323BB8`, `sub_823227F8`,
`sub_82323468`, `sub_820D0F20`, and others) — ordinary per-tick
animation/clip update churn, matching `6d61b5cd`'s characterization,
continuing unbroken across the press.

## What actually stops: `sub_82322A80` alone, exactly at the press

`704b27b6` identified `sub_82322A80` as the one function that walks a
cursor and calls `Add()` to enqueue a script statement — its only
observed caller in this campaign is one branch of a per-element type
dispatch (a "third jump table," `704b27b6`), reached from this owner's
own per-tick update. In this run: **857 calls before tick 3000, exactly 2
at tick 3000 itself, and zero from tick 3001 through the end of the run
(tick 3599)** — while every other writer function in the same bracket
keeps firing normally for the rest of the run.

## Reading

**The stop is precisely localized, for the first time**: not object
destruction (ruled out this report), not the owner going idle (ruled out
— it stays intensely active), but specifically the type-dispatch branch
that used to route into `sub_82322A80` no longer being selected, for
whatever per-element/per-frame reason drives that dispatch. This
completes `74756ffc`'s open question in the negative (owner persists) and
sharpens `704b27b6`'s own "Not established" item (table 3's dispatch site
and selector) into the single most specific remaining fact: whatever
selects this dispatch branch changed state at tick 3000/3001, and nothing
about the owner's own lifecycle explains why.

**A plausible but unconfirmed reading, stated as a hypothesis, not a
finding**: the campaign has independently established this whole
subsystem carries genuine ActionScript/Flash-movie-clip vocabulary
(`_x`/`_y`/`_currentframe`/`_totalframes`, `getProperty`-shaped natives,
`003daa94`). A per-element type dispatch that simply stops selecting one
branch after an input event is exactly the ordinary behavior of a
timeline-driven movie clip changing frame/state in response to input —
not necessarily evidence of a missing kernel call or unconstructed
object at all. If correct, "what enqueues `EndMode`" may not be a
missing mechanism so much as a **frame/state the attract movie's own
timeline never returns to** after a START press, by original design —
which would reframe the whole investigation from "find the broken
trigger" to "find what timeline state the game expects the *next* screen
(a real menu or loading clip) to install, since attract's own clip isn't
meant to call `EndMode` again once left." Not verified; the dispatch
selector itself (what "type" value routes to `sub_82322A80`'s table slot,
and what changes it) has still not been read.

## Not established

- What specific value (element type tag, frame index, or similar) selects
  the dispatch branch containing `sub_82322A80`, or what changes that
  value at tick 3000/3001 — the concrete next static/live read, not yet
  done.
- Whether this same stop-at-press pattern holds for the *other* owner
  (`0x2E3EDA90`, `e4e9b251`) — not checked; that owner's own header was
  not bracketed this report, only its array.
- The Flash/movie-clip framing above — plausible given prior evidence,
  not confirmed by anything read in this report specifically.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. No source change — pure live trace with an existing,
unmodified instrument on a new address bracket.
