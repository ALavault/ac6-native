# AC6 retail NTSC-U/J — the ring push runs before its own heap is created, in program order (r81)

Date: 2026-08-31.

## Method

Continuation of r80 (live GDB read of the stalled object, `object=0x1a0010`,
`+0x2a90` null because its allocator returned null). This cycle traced the
allocator chain one level further, statically, then confirmed live.

`sub_821D74A8` (r80) loads its heap handle from a fixed guest global via
`lis r11,-0x7d6c` (`=0x82940000`) then `lwz r3,-0x4690(r11)` — global address
**`0x8293B970`**. `Ghidra`'s reference database has zero entries for this
address (consistent with `INSTRUMENT_DISCIPLINE.md`'s "the Xenon project has
no reference database" warning); `tools/find_materialised_address.py` also
found no `lis`+`addi/ori` pair building this constant within a 64-instruction
lookahead, because the address is never materialized into a register at all
— every reference folds it directly into a load/store's displacement field
(`FindStoresAtDisplacement.java -0x4690` was the search that actually found
it: one hit).

## Result

- **Live read** (same GDB technique as r80): guest address `0x8293B970` is
  **entirely zero** (32 bytes read, all zero) at the moment of the stall.
- **Static**: the sole writer of `-0x4690(r31)` in the whole image is
  `sub_821D5F48:0x821d6200` — and `sub_821D5F48` is already an ancestor
  frame in our own captured backtrace (`sub_821D5F48 → sub_82331CA8 →
  sub_8233C788 → sub_8234F288 → sub_8234F2C8 → sub_821E65B0 →
  sub_821E64A8 → sub_821E61A8 → sub_821E6AC8`, per r78/r80).
- Within `sub_821D5F48` (821 instructions, `0x821d5f48..0x821d6c1b`), the
  call that descends into the observed stall subtree is
  **`0x821d6008: bl 0x82331ca8`** (the only call to `sub_82331CA8` in the
  function). The heap-handle creation and store is
  **`0x821d61c8: bl 0x82222d80`** (creates a sub-heap from an already-ready
  parent handle at `-0x7150(r24)`) followed by **`0x821d6200: stw
  r11,-0x4690(r31)`** — **488 bytes, i.e. later in straight-line program
  order**, with no loop or branch between the two that would let
  `0x821d6008` run again after `0x821d6200` on this path.

`0x8293B970` is not "not yet initialized because of a slow background
thread" — it is initialized **later in the same function, on the same
thread**, after the call that (through several more frames) reaches the
ring push that depends on it.

## Not established

- Whether `0x821d6008`'s call to `sub_82331CA8` is supposed to reach the
  ring-push code path on this particular invocation at all on real
  hardware/Xenia, or whether `sub_82331CA8` is a general-purpose
  entry point normally gated by state that isn't yet reachable this early
  (a flag, a subsystem-ready check) which our native runtime may be
  satisfying prematurely somewhere upstream. A retail-shipped title would
  not normally have a straight-line ordering bug this blunt, so the more
  likely explanation is that some condition our native HLE stubs return
  "true"/"ready" for is meant to still read "false" at this point in real
  execution — but this is not yet traced.
- Whether other call sites into this same subtree (later, from elsewhere,
  after `0x8293B970` is genuinely populated) exist and are what real
  hardware actually uses; `sub_821E64A8`'s 8 callers (r79) were never
  individually checked for which are reachable only after full boot.

## Decision

Still no implementation. Writing a synthetic non-null value into
`object+0x2a90` (or into `0x8293B970` directly) would hide a real ordering
bug rather than fix it, and would violate this thread's repeated commitment
against synthetic state (r53, r77, r79, r80). The next cycle's task is
static: find what gates `sub_82331CA8` (or an ancestor of it, up to
`_xstart`) from running before `0x821d6008`'s point in real execution —
likely a condition check on a flag/counter the native runtime's HLE stub
layer sets differently than real hardware would this early in boot — and
only then decide whether the fix belongs in stub sequencing/timing rather
than in this ring code at all.

No native code changed. Throwaway single-purpose dump scripts used this
cycle (`DumpUsHeapAllocator.java`, `DumpUsHeapInitSite.java`) were removed
after use, per r79/r80 practice; `find_materialised_address.py` (existing
project tool) and `FindStoresAtDisplacement.java` (existing) were reused,
no new scripts kept.
