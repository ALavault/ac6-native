# `XamUserReadProfileSettings` materialized and the XGI validator's stack-padding
# word fixed clear both traps -- the falsifier now runs the full probe budget

## Qualification

Ghidra project `ghidra-projects/ace-combat-6-demo` (`PowerPC:BE:64:Xenon`). XEX
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, base
`0x82000000`. No oracle. Three live probe runs against
`recompilation/ace-combat-6-demo/build-codegen-on/ac6-demo-recomp probe`,
neutral store, the same `AC6_DEMO_FORCE_MENU_ENDMODE_ARG=1
AC6_DEMO_FORCE_MENU_ENDMODE_AT_TICK=4251` falsifier `1edce620` established.
Source changes: `recompilation/ace-combat-6-demo/src/guest_bridge/xam_bootstrap_dispatch.hpp`,
`recompilation/ace-combat-6-demo/src/guest_bridge_resources.cpp`.

## Correction to `fd119ba4`

`fd119ba4` reported the tick-4254 trap as an "unimplemented import." It is
not: `XamUserReadProfileSettings` already had a handler
(`xam_bootstrap_dispatch.hpp:227`, commit `70336364a`, predating this whole
session), which already reverse-engineered the guest's 9-register
`XUserReadProfileSettingsByXuid`-shaped ABI (`r5`/`r6` = `dwNumFor`/`pxuidFor`,
both zero for the local-user path) and already answers the size-only first
call correctly. Its own comment named exactly what was missing: "a later
materialization call must be qualified separately if the demo reaches one."
The falsifier's forced state advance is precisely what makes the demo reach
that second call.

## Fix 1: the materialization branch

Read `XUSER_READ_PROFILE_SETTING_RESULT.md`, `XUSER_PROFILE_SETTING.md`,
`XUSER_DATA.md`, `XUSER_PROFILE_SOURCE.md`, `XuserGetProfileSettingsType.md`
(`sdk/xdk-xenon-6132.6/XDK/doc/1033/markdown`, en-US) before writing anything.
Header is `{DWORD dwSettingsLen; XUSER_PROFILE_SETTING *pSettings;}` (8
bytes); each `XUSER_PROFILE_SETTING` is `{XUSER_PROFILE_SOURCE source;
union{DWORD;XUID} user; DWORD dwSettingId; XUSER_DATA data;}` = 4 + pad4 + 8 +
4 + pad4 + 16 = 40 bytes -- this matches the pre-existing handler's own
`needed_size = 8 + setting_count * 40` sizing formula exactly, corroborating
both the earlier author's math and this session's layout re-derivation
independently.

The offline demo store has no profile database, so every requested setting is
answered honestly as absent rather than fabricated: `source =
XSOURCE_NO_VALUE` (0, "There is no value to read") and `data.type =
XUSER_DATA_TYPE_NULL` (0xFF, the sentinel `XuserGetProfileSettingsType.md`
documents separately from the 0-7 type-nibble range). `dwUserIndex` and
`dwSettingId` are still echoed per entry so the guest can correlate each
result to its request. Implementation zero-fills each 40-byte entry, then
overwrites the three meaningful fields; returns `ERROR_SUCCESS` (0).

## Result 1: a new frontier, XMsgStartIORequest (ordinal 503)

Rebuilt (`build-codegen-on --target ac6-demo-recomp`), reran the falsifier
recipe. Advanced three ticks past the old trap (4251 to 4254, same as before)
to a **different** trap:

```
AC6 runtime trap: unimplemented import xam.xex ordinal 503 tick=4254 lr=0x821a55a0
```

This too already had a handler (`xam_bootstrap_dispatch.hpp:296`,
`validate_xgi_user_context_request` in `guest_bridge_resources.cpp:85`, same
`70336364a` commit) -- a strict fail-closed allowlist requiring an exact
match on `caller_lr`, `app`, `message`, `overlapped`, `length`, and all six
request-buffer words. It trapped because the buffer content did not match the
one previously-qualified tuple.

## Diagnosis: the mismatching word is guest stack padding, not message content

Added a narrow, read-only, env-gated diagnostic
(`AC6_DEMO_WATCH_XGI_REQUEST`) dumping the six request words whenever this
call is reached, regardless of validation outcome. Reran:

```
AC6_XGI_REQUEST tick=4254 lr=0x821A55A0 app=0xFB message=0xB0006 overlapped=0x0
  buffer=0x7F0409B8 length=24
  words=[0x00000000,0x18980054,0x00000000,0x00000000,0x00008001,0x00000000]
```

`app`, `message`, `overlapped`, `length` all match the qualified tuple
exactly. Words 0, 2, 3, 4, 5 also match. Only word 1 (`buffer+4`) differs:
`0x18980054` here versus `0x00000000` in the tuple `70336364a` qualified.

Read the sole caller, `sub_821A5550`
(`ppc_recomp.18.cpp:12470-12548`, LR `0x821A55A0`), in full. It builds the
24-byte request on its own stack at `r1+80`:

```
stw r11,80(r1)   // word 0 = entry r3
std r11,88(r1)   // words 2-3 (offset 88, 8 bytes) = 0 (r11 was li'd to 0 first)
stw r10,96(r1)   // word 4 = entry r4
stw r9,100(r1)   // word 5 = entry r5
```

Offset 84 (word 1) is never stored to by this function. It is whatever the
stack held at that slot before the call -- confirmed uninitialized guest
stack padding, not a field of the XGI message. The two live captures (0x0 in
the tuple `70336364a` qualified, `0x18980054` -- itself a live pointer
resembling `manager+0x54` from the tick-107 `AC6_MODE_REQUEST manager=0x18980000`
trace -- in this run) are two different leftover stack contents from two
different call histories reaching the same LR, exactly as expected for an
uninitialized read. `sub_821A5550` also never re-reads offset 84 after the
call (it only branches on the returned `r3`), and the existing handler
comment already establishes Xenia's own `XgiApp` handler for this message
"only validates/logs" and produces no output the guest consumes -- so this
word carries no signal either into or out of the call.

## Fix 2: exclude the padding word from the validator

`validate_xgi_user_context_request` now skips index 1 in its per-word
comparison loop, with the derivation above recorded in a comment at the call
site. Every other field remains pinned to the exact previously-qualified
value; nothing about the fail-closed discipline for the five real fields
changed.

## Result 2: the full probe budget, zero traps

Rebuilt both trees (`build-codegen-on` and `build`), ran the full gate
sequence (native gate JF pass, `ctest` 26/26, both contract audits pass, no
unrelated tracked-file drift after `ctest`). Reran the identical falsifier
recipe a third time:

```
outcome.kind = "max_ticks"
completed_ticks = 6000
milestones = {presents: 5863, frontend: false, mission: false, terminal: false}
```

**No trap.** This is the first time in the whole `menu_endMode`/EndMode/XGI
thread (this session and its predecessor) that this falsifier has run to its
full tick budget rather than stopping on a trap. Presents rose from the
untouched baseline's 5463/5348-tick range to 5863 across the same 6000-tick
window -- the guest is doing real, sustained work, not idling.

From roughly tick 4500 onward the trace settles into a steady per-tick `swg`
native call:

```
AC6_SWG_NATIVE_CALL tick=<N> thread=1 target=0x820E9838 table_row=0x82386478
  context=0x2E3FA914 arg_count=1 args=<rotating> first_arg=0x2E415B90 tag=M150
```

`args` rotates across a small set of addresses (`0x2E415794`, `0x2E415894`,
`0x2E415914` observed) tick over tick -- consistent with a multi-buffer
animation or per-frame update step, not a stuck/repeating no-op, though this
is pattern-matching on the trace shape, not a read of `0x820E9838` or the
`M150` tag's meaning.

## Consequence for the plan

`5dc58584`'s `CX360UnitManager` priority stands as a separate, still-open
question (Phase 2/3 territory per the plan). This result is Phase 1 progress
on a different, independently-reachable path: two real kernel-boundary gaps
in an already-partially-implemented handler pair, both closed with
evidence read before being written, no fabricated content, no host side
effects invented beyond what each call's own ABI already specified. The
falsifier no longer measures "does it trap" -- it measures "what does the
tick-4500+ loop do," which is the next question.

## Not established

- What `0x820E9838` (the `M150`-tagged native call target) is or does --
  not read this cycle.
- Whether `frontend=true` is reachable by continuing to run past 6000 ticks
  from this exact state, or whether the tick-4500+ loop is a genuine
  steady-state (e.g. an attract/idle animation) that needs a different input
  (a START press, as `AC6_DEMO_START_DURING_TITLE.md` established works
  during the title window) to progress further.
- Whether the rotating `args` addresses correspond to distinct logical
  buffers (e.g. double/triple animation buffering) or to something else.
- Whether any further import gap exists past tick 6000 -- this run's budget
  was exhausted, not its progress.

## Process note

`git log --oneline --reverse fd119ba4..HEAD` is empty -- `fd119ba4` is `HEAD`.
This report supersedes its "unimplemented import" framing with the more
precise "unimplemented branch of an already-partially-implemented handler"
finding, and extends past it to a second, independent fix in a sibling
handler from the same original commit.
