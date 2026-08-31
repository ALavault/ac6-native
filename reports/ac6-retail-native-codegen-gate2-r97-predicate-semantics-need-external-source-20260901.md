# AC6 retail NTSC-U/J — predicate execution needs an external source; two known-opcode blockers and 257 unimplemented-opcode packets precisely mapped (r97)

Date: 2026-09-01.

## Qualification

No Ghidra pass this cycle. No oracle used. No native code changed — this
cycle is investigation-only, deliberately, for the reason explained
below. No commit to production code; report and trackers only.

## Continuing r96's frontier

r96 named "implement predicated `TYPE3` execution" as the next frontier.
Before touching code, this cycle characterized precisely what that would
actually require and cost, using the same 2840-dword indirect-buffer
capture (guest `0x125c0000`) r96 already dumped — no new probe needed.

## What the buffer actually contains

Parsed the full stream (Python, the project's own PM4 field-decode logic)
tracking every `TYPE3` opcode, its predicate bit, and whether this
decoder already implements it (cross-checked against every
`case pm4::kOpcode...` in `native_xenos.cpp`'s switch, not just the
constant list — a constant existing does not mean the switch handles it,
though in this case all relevant ones do):

- **281 `TYPE3` packets total**; only **2** have the predicate bit set:
  offset 239, `DRAW_INDX_2` (`0x36`, already implemented), and offset
  294, `WAIT_REG_MEM` (`0x3c`, already implemented).
- The first **genuinely unimplemented** opcode (not in
  `native_xenos.h`'s `kOpcode*` list at all) is `0x46` at offset 400 —
  reached only *after* both predicated packets, meaning fixing the
  predicate rejection alone would let decode advance from offset 239 all
  the way to offset 400 (161 more packets) before hitting a different
  wall.
- Opcode `0x45` then repeats **257 times** through the rest of the
  buffer — by far the dominant unknown opcode in this content, and the
  real scale of what "finish decoding this buffer" requires.

## Why the predicate fix is not implemented this cycle

`grep -rn "predicat" native/src native/include` finds **exactly one**
match in the whole codebase: the rejection message itself
(`native_xenos.cpp:171`). There is no predicate register, no
predication-enable flag, no occlusion-query state — nothing to condition
a skip-vs-execute decision on. The existing pre-session test
(`decoder_enforces_hardware_predicate_and_one_register`,
`native_xenos_tests.cpp:92-105`) asserts this exact rejection as correct
behavior, which reads as a deliberate "reject rather than guess" design
choice by whoever wrote it, not an oversight.

Real Xenos/R500-class GPU command processors treat a packet's per-packet
predicate bit as significant only when a separate, persistent
predication-enable state has been turned on by an earlier command; with
predication never enabled, hardware executes every packet unconditionally
regardless of the bit. That is architecturally plausible and would be a
minimal, bounded fix (ignore the bit, decode normally) — but it is
**not verified against this codebase, this game's actual command stream,
or any cited Xenos hardware documentation available in this workspace**.
Implementing it on that basis alone would be exactly the "plausible rule
with no control" this project's evidence discipline refuses (r79, r111,
r113's standard), and the failure mode if wrong is worse than the current
state: silent incorrect rendering (drawing geometry that should have been
occlusion-culled, or skipping a real synchronization wait) instead of a
loud, diagnosable rejection.

**This needs an external source** — genuine Xenos/R500 PM4 predication
documentation, or an oracle comparison — neither available in this
session (the project's oracle policy has been "no" for the entire
campaign, per `AGENTS.md`/`CLAUDE.md`). Flagging this precisely rather
than guessing.

## What is safe to do without that source

Nothing was changed this cycle. If a future cycle gets access to a
verified source, the fix is narrowly scoped: strip the predicate bit
check at `native_xenos.cpp:169-171` (or implement real
predication-enable tracking if the verified semantics require it) —
localized to the two packet types already confirmed to appear predicated
in real content (`DRAW_INDX_2`, `WAIT_REG_MEM`).

Separately, and not blocked by the predicate question: opcodes `0x45`
and `0x46` are real, frequently-used, entirely unimplemented PM4 opcodes
in this content. Identifying their semantics (from the retail
disassembly's own command-stream construction code, which is a bounded,
verifiable static task unlike guessing hardware documentation) is a
distinct, well-scoped next step that does not carry the same
"could silently corrupt rendering" risk as guessing predicate execution,
since an unimplemented opcode currently fails loudly rather than
executing incorrectly.

## Gates

- No native code changed; `ctest`/pytest not re-run (no build to
  invalidate). `git status` after this cycle's edits shows only tracker
  and report files.
- `audit_ac6_mission01_native_gate.py`: unchanged from r90-r96 (same
  pre-existing, unrelated N2 mismatch).

## Next

Two independent, unblocked options, neither requiring the predicate
question to be resolved first:
1. Find or obtain a verified source for Xenos PM4 predication semantics,
   then implement the narrowly-scoped fix named above.
2. Investigate opcodes `0x45`/`0x46` from the retail guest disassembly
   (find the code that constructs these PM4 packets, to derive their
   real payload semantics statically rather than guess) — likely the
   higher-value target given `0x45` alone accounts for 257 of 281
   packets in this one buffer.
