# State 1 is a dead end inside the loading task's own update function

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`).
XEX `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Static read of `sub_8217E3E0`
(`ppc_recomp.14.cpp:32818-33125`) in full, plus vtable dumps against
`.build/Default.xex.base.bin`. No probe run, no source change.

## What this checks

`69e3435f` named two candidates for what consumes `SendMsgI("M150")`'s now-
proven-correct "ready" answer: the update function's own `[this+12]`-
switched state machine (per `ea9b3a6a`'s sibling-class pattern), or the
script's own bytecode. This report reads the update function candidate to
completion.

## Finding the function

`ea9b3a6a`'s `sub_8218A4A0` (startup's own `[this+12]`-switched update) sits
at slot `+0x10` in startup's primary vtable, `0x8201130C`. Dumping both
vtables directly: loading's primary, `0x82011154` (`994109dc`'s
`AC6_MODE_SWITCH` line), has `0x8217E3E0` at the identical slot `+0x10` --
the loading task's own analogous update function.

## `sub_8217E3E0`, read to completion

Switches on `[this+12]` (`r11`, the same word `AC6_MODE_INNER` tracks,
confirmed identical by `69e3435f`): `<1` -> `loc_8217E51C` (state 0);
`==1` -> `loc_8217E42C` (state 1); `==2` -> `loc_8217E4F8`; `==4` ->
`loc_8217E444`; `==5` -> calls `sub_8217E900`; other -> `loc_8217E5F4`
(the function's tail).

**State 1's entire body** (`loc_8217E42C`): reads `[[this+28]]+32` (a
vtable slot on a *different* sub-object at `this+28`), calls it via
`bctrl`, then falls straight to `loc_8217E5F4` -- the function's epilogue
and `return`, confirmed by reading it directly (`addi r1,r1,128; b
0x82327158; return`), nothing else. **No comparison, no write, no branch
anywhere in this path touches `[this+12]`.** This exactly matches the live
`M150` broadcast this campaign has watched every tick since tick 5414: a
per-tick callback dispatch, nothing more.

**State 2** (`loc_8217E4F8`): decrements a countdown at `[this+68]` (the
*same offset* `ea9b3a6a` found for startup's own countdown); when it
reaches zero, writes `1` to `[[0x827435F8]+24]` -- `CTaskModeManager`'s
request field, the exact mechanism `ea9b3a6a` documented for startup
requesting its own mode transition. **This state is never observed live**
(`994109dc`'s `AC6_MODE_INNER` trace for this object shows `0 -> 4 -> 1`
directly; `2` never appears) -- present in the code, unreachable on this
route.

**State 4** (`loc_8217E444`): the only place in the whole function that
writes `[this+12]` to `1`. Gated on: a lookup (`sub_8218CCD0`) against a
table at `[[0x827435F8]+13816-derived-offset]` returning `>0`; a
*second, independent* read of the exact flag byte gate 3 already reads
(`[[0x827435F8]+0x222BFE]`, nonzero); `[this+136]==0`; and finally
`[this+132]!=1` (`loc_8217E4DC`'s branch to `loc_8217E424`, `r11=1`,
stored). The alternate branch (`[this+132]==1`) calls `sub_8217E8C8` and
sets `[this+12]=5` instead. Neither `sub_8218CCD0`, `sub_821A1E08`,
`sub_82095B80`/`sub_8218CBD8` (state 4's other call chain), `sub_8217E900`
(state 5), nor `sub_8217E8C8` were opened this cycle -- their bodies are
not needed to answer the question this report was reading for.

## Conclusion

**`[this+12]` reaching `1` (the observed tick-5414 transition) is state 4's
own doing, not an external signal — confirmed, state 4's own logic is
what writes it.** But **once inside state 1, `sub_8217E3E0` has no path,
anywhere in the function, that can move `[this+12]` away from `1` again.**
Read exhaustively: every `stw ...,12(r28)` site in this function
(`loc_8217E428`, reached only from state 4's or state 5's own branches)
writes `1` or `5` -- both approach or re-enter state 1's shape, none
originates *from* state 1 to leave it. **Candidate (b) from `69e3435f` is
now ruled out by direct code reading, not inference: this function cannot
be what consumes the `SendMsgI` "ready" answer to advance the mode,
because this function has no advance path out of state 1 at all.**

Whatever should move this mode forward once `SendMsgI("M150")` genuinely
answers "ready" is not in `sub_8217E3E0`. It is either the `[this+28]`
vtable-slot-`+32` callback state 1 dispatches every tick (a *different*
object, its own class and vtable not yet identified -- no live pointer for
`[this+28]` has been captured), or the script bytecode itself (`69e3435f`'s
candidate (a), context `0x2E3FA914`).

## Consequence for the plan

This closes the update-function candidate cleanly and narrows the
remaining open question to two, ordered by cost: (1) identify and read
whatever class sits at `[0x2E3C0200+28]` and its own `+32` vtable slot --
cheap if a live pointer is captured, since RTTI-walking is now routine for
this campaign; (2) failing that, reconstruct and read the `M150`-sending
script's own bytecode via the `346255b2`/`1fcc88b3` method already built
for a different context, applied to context `0x2E3FA914`.

## Not established

- What class sits at `[0x2E3C0200+28]`, or what its `+32` slot does --
  needs one live pointer read, not yet captured.
- Whether `sub_8218CCD0`'s lookup, `sub_821A1E08`, `sub_82095B80`/
  `sub_8218CBD8`, `sub_8217E8C8`, or `sub_8217E900` (state 4/5's other
  call targets) touch anything relevant -- not read, judged unnecessary
  for this report's specific question.
- Whether state 2's countdown-and-request mechanism is reachable under any
  route this campaign hasn't yet tried -- observed unreached on the one
  route measured, not proven unreachable in general.

## Gates

Native gate JF, demo `ctest` 26/26, both contract audits: run clean below
before this commit. No source change -- pure static read.
