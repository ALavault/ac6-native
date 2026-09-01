# AC6 retail NTSC-U/J — user-authorized investment: full Ghidra analysis resolves the `sub_821F5630` indirect-call mystery; a real Xenia oracle run against the correct NTSC-U/J ISO confirms native-Linux Xenia's documented stall applies here too (r156)

Date: 2026-09-01.

## Qualification

Ghidra project: a scratch **copy** of `ghidra-projects/ac6-us`
(`/fastdata/tmp/claude-1007/r156-ghidra-analysis/`, session-local, not
committed) — deliberately not the canonical read-only project, to avoid
mutating shared state other cycles depend on (`identity.json` records it as
`analysis: loader-only`). XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. Oracle:
a real, freshly-launched Xenia Canary (native Linux build,
`.tools/xenia-canary/build/bin/Linux/Release/xenia_canary`) against the
**correct** retail NTSC-U/J ISO
(`204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`), not
the PAL `game-files/default.xex`
(`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`) the
existing `scripts/run_xenia_ac6_oracle_baseline.sh` and
`XENIA_WINE_ORACLE_HANDOFF.md` are wired for. Both investments were
explicitly authorized by the user this cycle ("Oracle and invest"), the
first genuine crossing of this campaign's own "no oracle used" line since it
began. No native code changed. No files under version control were
modified by either investment; the Xenia session and Ghidra scratch copy
both live under the session scratch directory.

## Investment 1: full Ghidra `-analysis` resolves r154/r155's mystery

r155 found that a Ghidra xref scan run `-noanalysis` (this project's usual
mode, for speed) reported `sub_82390880` as the sole static caller of
`NtQueryInformationFile`'s import thunk (`0x823D031C`), yet live
instrumentation proved `sub_82390880` is never entered on a run where the
import still fires — and speculated, without verifying, that a
`-noanalysis` database misses indirect/computed calls.

A full `-analysis` pass (`analyzeHeadless` without `-noanalysis`, ~192s) on
the scratch copy confirms exactly that, concretely:

```
=== xrefs to 823D031C ===
  823908a4  UNCONDITIONAL_CALL  in Function_82390880@82390880  bl 0x823d031c
  82392110  COMPUTED_CALL  in Function_82392040@82392040  bctrl
  82391f84  COMPUTED_CALL  in Function_82391F38@82391f38  bctrl
  82391fbc  COMPUTED_CALL  in Function_82391F38@82391f38  bctrl
  821f532c  COMPUTED_CALL  in Function_821F52C0@821f52c0  bctrl
  821f536c  COMPUTED_CALL  in Function_821F52C0@821f52c0  bctrl
  821f5664  COMPUTED_CALL  in Function_821F5630@821f5630  bctrl
  823f07c0  DATA  in none  (data)
  total 8
```

Seven new references appear, all `COMPUTED_CALL` (i.e. `bctrl` — an
indirect call through the count register) — invisible both to the
`-noanalysis` reference database and to this session's literal-text grep of
the generated C++ (which can only match a direct, named call like
`NtQueryInformationFile(ctx`, never a `PPC_CALL_INDIRECT_FUNC(ctr.u32)`).
**`sub_821F5630` — exactly the function r154's `backtrace()` capture named
— is confirmed as a real, direct (if indirect-dispatch) caller.** Reading
its body confirms the mechanism:

```cpp
// lis r11,-32193
r11.s64 = -2109800448;
...
// lwz r11,1996(r11)
r11.u64 = PPC_LOAD_U32(r11.u32 + 1996);
...
// lwz r11,32(r11)
r11.u64 = PPC_LOAD_U32(r11.u32 + 32);
// mtctr r11
ctr.u64 = r11.u64;
// bctrl
PPC_CALL_INDIRECT_FUNC(ctr.u32);
```

This is a vtable-style double indirection: a fixed global object pointer,
dereferenced for a per-call sub-object (`r31`/`ctx.r4`, the "provider" this
call operates on), then a function pointer read from offset `+32` within
that sub-object and called indirectly. `sub_821F5630` is a **generic
provider-dispatch wrapper**, not a hardcoded caller of
`NtQueryInformationFile` — for whichever provider object is passed in, it
calls whatever method sits at that vtable slot, and for `DATA.TBL`'s
provider that slot happens to be (or forward to) `NtQueryInformationFile`.

**This corrects r154's own hypothesis.** r154 guessed "tail-call elision"
(`-O3` eliding real intermediate frames) as the reason a literal source
check of `sub_821F5630` and `sub_821F75B8` found no direct call. That guess
was wrong: there was no elision. The real reason is structural — an
indirect call cannot appear as literal call text in generated C++ no matter
how carefully the source is read, and r154's own methodology note ("cross-
check backtrace frames against a literal call") was sound advice applied to
the wrong diagnosis. The correct lesson, updated here: when a literal-text
check finds no call but a live backtrace names a real function, check for
an indirect dispatch (`bctrl`/`PPC_CALL_INDIRECT_FUNC`) before concluding
optimization elided the frame.

This also refines r150's own open caveat: r150 found `NtQueryInformationFile`
now firing on `DATA.TBL`'s own handle and could not say whether r147's
"movie-capture debug feature" characterization (traced through
`sub_82390880`'s **direct** call) still described this case. It does not —
`sub_82390880` (direct `bl`) and `sub_821F5630` (indirect `bctrl` through a
provider vtable) are two structurally different, unrelated call sites to
the same import thunk. r147's movie-capture reading was correct for its own
site; it was never a description of what `DATA.TBL` reaches. `DATA.TBL`
goes through the generic provider dispatcher instead.

## Investment 2: a real Xenia oracle run against the correct NTSC-U/J ISO

`scripts/run_xenia_ac6_oracle_baseline.sh` and `XENIA_WINE_ORACLE_HANDOFF.md`
are both wired for the PAL title (`game-files/default.xex`,
`acc302c1...`), not this campaign's NTSC-U/J target. A fresh, isolated
Xenia Canary session (own `Xvfb :178`, own `HOME`/`XDG_*`, no reuse of any
existing display or profile) was launched directly against the correct
retail ISO
(`.../Ace Combat 6 - Fires of Liberation (USA, Japan) (En,Fr,De,Es,It).iso`,
matching the sealed identity's `204c5e6...` hash).

The session progressed identically to a prior, already-recorded attempt
(`analysis/oracle/ac6-recomp-ab90b-us/identity.json`'s
`xenia_identity_cross_check`, and this machine's pre-existing
`.tools/xenia-canary/build/bin/Linux/Release/xenia.log` from an earlier
"baseline-host-audio" retest): thread spawn proceeds normally, VFS resolves
paths against the disc image (`DiscImageDevice::ResolvePath()`), then:

```
ALSA lib pcm.c:2722:(snd_pcm_open_noupdate) [error.pcm] Unknown PCM cards.pcm.surround51
!> F8000008 SDL_OpenAudioDevice() failed.
!> F8000008 AudioSystem::RegisterClient: CreateDriver failed for index=0
```

No further log output for the next ~7 minutes 20 seconds of wall-clock time
(464s elapsed), with the process holding ~20-30% CPU (a live thread, not
crashed or hung on I/O) and the GTK window showing its menu bar over an
otherwise fully black render surface — a screenshot was captured confirming
this. This exactly matches `XENIA_WINE_ORACLE_HANDOFF.md`'s own prior
finding for the PAL route: *"On this host, the native build launched the
module but remained black. The qualified interactive route is the pinned
Windows build through Wine with Vulkan."* **This cycle confirms the same
limitation holds for the NTSC-U/J retail title, not just PAL** — a new,
concrete data point this session, not merely a re-read of the existing
note. The session was killed rather than left running further, since the
log had gone fully quiet and CPU usage suggested a busy-wait rather than
forward progress (consistent with the audio-device failure blocking a
synchronization point some other thread waits on).

## What this does and does not establish

**Establishes**: `sub_82390880`/`NtQueryInformationFile` caller-identity
question (open since r150, worked on across r154/r155/r156) is now fully
closed with a verified, decisive answer — `sub_821F5630`'s generic provider
dispatch, not `sub_82390880`'s movie-capture idiom, is what `DATA.TBL`'s
handle actually reaches. r147's characterization stands for its own call
site but does not apply to `DATA.TBL`.

**Does not establish**: any new information about the tracked
`sub_821F7C80` crash or the DATA.TBL allocation-size chain (r150-r153) —
this was a separate, already-closed question. Does not establish real
Xbox 360 hardware behavior for the uninitialized `[r1+88]` stack slot
(r153) — the native-Linux Xenia route cannot answer that while it stalls
before reaching the relevant code path at all; only the Wine/Windows route
(documented, PAL-only so far) is qualified for interactive progress on this
host, and standing it up for NTSC-U/J is a separate, not-yet-attempted
investment.

## Decision

The `sub_821F5630` finding is a genuine, verified resolution — recorded as
such, closing the last open item from r154/r155 with an actual answer
rather than a documented dead end. The Xenia investment produced a real,
useful negative result (native-Linux Xenia is confirmed non-viable for
*this* title too, not just PAL) but did not reach the DATA.TBL/gameplay
question it was aimed at. Standing up the Wine-based route for NTSC-U/J —
a materially larger effort (new profile/save setup, keyboard-driven menu
navigation, sustained interactive session) — is named as a distinct,
separately-scoped next investment rather than attempted opportunistically
this cycle.

## Gates

No native code changed this cycle; the retail-native build/tests were not
touched, so `ctest`'s last-known state (9/9, r155) stands unaffected.
`git status` unchanged (only pre-existing, unrelated dirty state). Neither
investment touched anything under version control.

## Next

1. If further oracle evidence for the DATA.TBL/uninitialized-stack question
   is wanted, stand up the Wine-based Xenia route for the NTSC-U/J retail
   title specifically (new profile, save data, and interactive session) —
   a distinct, larger investment from this cycle's.
2. No further action needed on the `sub_82390880`/`sub_821F5630` caller
   question; it is closed with a verified answer.
3. Both of Gate 2's named frontiers remain otherwise unchanged: DATA.TBL
   chain fully traced (r144/r153), `IM_LOAD_IMMEDIATE`→SPIR-V still
   policy-blocked pending real oracle qualification of Xenos fetch
   signatures — this cycle's oracle investment did not reach that
   question either.
