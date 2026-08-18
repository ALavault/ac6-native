# Correcting `GETCURRENTMISSION_IS_ALWAYS_16`: the branch was read backwards

## Qualification

AC6 demo PAL, same XEX SHA-256. No new run. Static re-read of generated code
already in the tree: `sub_820EA550` (`GetCurrentMission`'s handler,
`ppc_recomp.6.cpp:23263-23310`), `sub_820E9300` (the gate it calls,
`ppc_recomp.6.cpp:20443-20522`), `sub_82095B80` (the value it actually
returns on the branch that matters, `ppc_recomp.0.cpp:13823-13891`), plus
`sub_820EA538`/`sub_820E9290`/`sub_820EA598` (Mode/Level, re-checked for the
same failure mode). No oracle.

## What this corrects

`AC6_DEMO_THE_SCRIPT_VOCABULARY.md:70` states, unmeasured and flagged as such
in its own "Non établi" section: `GetCurrentMission = sub_82095B80(gs+112),
forcé à 16 si sub_820E9300(gs+112)`. `AC6_DEMO_GETCURRENTMISSION_IS_ALWAYS_16.md`
(`3c7e7291`) turned that into a live-sounding conclusion: "`[sub112+8]`'s
first check fails immediately... `GetCurrentMission()` is therefore forced to
its `16` fallback for the entire run." `AC6_DEMO_TITLES_FIVE_POST_START_NATIVE_CALLS_NAMED_NONE_IS_COMPLETION.md`
(`6e8fab2f`) repeated it and upgraded the status further: "falls back to `16`
when it fails — the same fallback value `3c7e7291` already measured live" —
`3c7e7291` never measured the return value at all, only `[sub112+8]`'s raw
contents. `AC6_DEMO_FORCING_SENDMSGIS_RESULT_DOES_NOT_TRIGGER_COMPLETION.md`
(`c73498cb`) then measured the real boxed value live (`0`, tick 3001),
flagged the conflict with "16", and left it as the explicit prerequisite for
the next step. This report is that prerequisite, closed.

## The read, corrected

`sub_820EA550` (`GetCurrentMission`):

```cpp
ctx.r30.u64 = sub_82095B80(gs+112);   // r30 = the "raw" answer, always computed
ctx.r3.u64  = sub_820E9300(gs+112);   // the gate: 0=fail, 1=success
ctx.r11.u64 = ctx.r3.u32 & 0xFF;
ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
if (ctx.cr0.eq) goto loc_820EA58C;    // gate==0 (FAIL) -> SKIP "r30=16", keep raw
ctx.r30.s64 = 16;                     // only reached when gate==1 (SUCCESS)
loc_820EA58C:
PPC_STORE_U32(ctx.r29.u32 + 0, ctx.r30.u32);   // store whichever r30 survived
```

The `beq` branches *around* `li r30,16` on gate failure, not onto it. Every
prior report in this chain read it the other way: "fails -> 16 fallback."
The generated C++ says the opposite — **16 is what gets returned when the
gate *passes*; on failure, the function returns whatever `sub_82095B80`
independently computed, before the gate was even called.**

`sub_820E9300` (the gate) is a clean boolean, confirmed by reading past the
line I had previously only partially traced: fails (`r3=0`) if
`[gs+112+8] != 1`, or `sub_82095B80()`'s own re-invocation inside the gate
`!= 2`, or a final table lookup at `[slot+1736]` is nonzero; succeeds
(`r3=1`, the `li r3,1` at `loc_820E934C`) only when all three hold.

`3c7e7291`'s own live measurement — `[sub112+8] = 0x821DFC00` for the entire
run, never `1` — is unchanged and still correct as a raw read. Fed into the
*correct* branch reading, it means the gate fails on its very first check,
every tick, both pre- and post-START. So `sub_820EA550` never takes the
`r30=16` line at all in this run — it always returns `sub_82095B80`'s raw
value.

## What `sub_82095B80` actually returns here

Read in full (`ppc_recomp.0.cpp:13823-13891`). It switches on the same
`[gs+112+8]` field: `2`/`3`/`4`/`5` each return a distinct fixed field
(`+12`/`+32`/`+28`/`+20`); anything else — the case that applies here, since
`0x821DFC00` matches none of the four — falls to a default path: read
`[gs+112+0x206E4]`, clamp it into `{0,1,2}` (negative or `>=3` forced to
`0`), multiply by `43704`, add to `gs+112`, and return `[computed_slot+1732]`.

This is a genuine per-slot state read, not a sentinel. Live measurement
(`c73498cb`, tick 3001) shows it evaluating to `0` in the current run —
consistent, not contradictory, with `3c7e7291`'s `[sub112+8]` reading, once
the branch is read correctly.

## Mode and Level, checked for the same failure mode

`sub_820EA538` (`GetCurrentMode`) is a direct, branch-free read of
`[gs+120]` — no gate to misread. Live `0` stands as measured.

`sub_820EA598` (`GetCurrentLevel`) calls `sub_820E9290(gs+112)`, structurally
the sibling of `sub_82095B80` (same switch shape on `[gs+112+8]`, same
default-path arithmetic, reading `[computed_slot+1744]` instead of `+1732`),
then remaps only `6->7`/`7->6` on the result — no `skip-an-assignment`
branch shape exists here to invert. `[gs+112+8]=0x821DFC00` matches none of
`sub_820E9290`'s special cases (`2`/`4`/`5`) either, so it also takes the
default path; the resulting value (live: `2`, tick 3001) isn't `6` or `7`,
so the remap is a no-op and the raw default-path read passes through
unchanged. `6e8fab2f`'s description of this handler ("remaps its result
before returning it") stands as written.

## Reconciled

`GetCurrentMode`, `GetCurrentMission`, and `GetCurrentLevel` all return, in
this run, ordinary values read from the same per-slot state block at
`gs+112` (`0` / `0` / `2`) — not hardcoded fallback sentinels forced by a
failed gate. `16` is real code, but it is the value the *healthy* path
produces, never observed live in this campaign. The distinction matters for
what comes next.

## Consequences

- **The falsifier this campaign was about to run needs re-aiming.** The
  natural next experiment is no longer "force a correct value instead of a
  broken fallback" — it's the opposite: force `GetCurrentMission`'s result to
  `16` (the gate-*success* value) and see whether that, or the specific
  `0`/`0`/`2` triple already in place, is what the script's branch actually
  keys on. `16` is now the more interesting probe value, not the thing being
  escaped from.
- **The "broken fallback answers" framing in `642f77a4`'s upstream
  hypothesis, repeated in `6e8fab2f`'s Conclusion ("results are wrong,
  forced fallback values, not what a correctly-initialized script would
  see"), is weaker than stated.** The script may be receiving perfectly
  ordinary, correctly-computed answers for a game-state block that
  legitimately says "still on the title screen, mission 0, level 2" — not
  wrong answers, right answers to "nothing selected yet." The open question
  shifts from *is the answer wrong* to *what would make this state block's
  fields change at all*, which is not yet investigated.
- **One narrow reopening, practically moot**: `3c7e7291`'s "constant across
  the press, therefore not the discriminator" argument no longer follows
  from the reasoning as written (it inferred a return value that was never
  the one actually returned), though its live-observed input
  (`[sub112+8]` constant) and the practical upshot (this campaign's 9-call
  trace shows each query asked exactly once, post-press) are untouched.

## Not established

- What `[gs+112+0x206E4]` (the slot selector both `sub_82095B80` and
  `sub_820E9290` clamp and index by) currently holds, or what selects it —
  not read.
- What `sub_82095B80(gs+112) == 2` (the gate's *second* condition, a
  re-invocation of the same dispatcher with a different implicit
  expectation) would require to hold, or whether it's reachable at all in
  this build — not traced.
- Whether forcing `GetCurrentMission` to `16` live changes the native-call
  trace — not yet run; this report is the reconciliation gating that
  experiment, not the experiment itself.

## Gates

No source changed; report-only commit. Native gate JF, demo `ctest`, and
both contract audits verified clean below before commit, per house rule.
