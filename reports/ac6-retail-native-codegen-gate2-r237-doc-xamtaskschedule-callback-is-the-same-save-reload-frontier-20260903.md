# AC6 retail NTSC-U/J — documentation only: `XamTaskSchedule`'s guest callback is the same deferred save/reload frontier, not a separate blocker (r237)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **No code change this cycle.** `ctest` 10/10, pytest 211/211
(unaffected — no source touched).

## What was found

r198 grouped `XamTaskSchedule`/`XamTaskCloseHandle` together as both
needing "a guest-callback execution subsystem this project has never
built." r230 already corrected the `CloseHandle` half (its own real
caller discards the return value, so it needed no such subsystem).
`XamTaskSchedule` itself was re-examined this cycle after noticing
`ExCreateThread`'s already-implemented `render_body` case (r114)
establishes a real, working precedent for a native import stub to call
back into recompiled guest PPC code: `PPC_LOOKUP_FUNC(base,
guest_address)` resolves a guest code address to a callable `PPCFunc*`,
which is then invoked with a `PPCContext` set up for the call. This
precedent means "needs a guest-callback subsystem" is no longer, by
itself, a reason to defer — the subsystem already exists.

`XamTaskSchedule` has exactly one real call site (`0x82391df0`, inside
`Function_82391A40`, the same large save-content-scan/write function
already central to r202's save/reload tracing and r229/r230's work).
Raw disassembly of the call site (`scripts/DumpRange.java`) resolves the
real signature precisely: `r3` = a fixed guest code address
(`0x823917f8`, a routine/callback pointer — the same address at every
invocation, since this is the only call site in the whole XEX), `r4` = a
context pointer (`r31`, itself computed as `r11+0x6310`), `r5` = a small
options/flags word (`0x02080002` in this call), `r6` = an out-pointer for
a handle the caller reads back and immediately passes to
`XamTaskCloseHandle` on success. The caller gates on `cmpwi r3,0; blt` —
non-negative is success — matching NTSTATUS convention.

Under the current generic offline default (negative `kOfflineStatus`),
the caller takes its own failure branch, skipping the handle read/close
— that much is already graceful, matching the pattern this sweep found
repeatedly. But execution continues past that branch regardless of
success or failure, and later reads a global (`uRam82916320`) that only
the real callback routine would ever set, using it to decide between an
error path (`Function_82391588()`) and marking the scan "completed"
(`iRam829162fc = 1`). If that global happens to be zero-initialized BSS
(the ordinary default for an untouched global), the current behavior
risks a **false-success signal**: the scan is marked complete even
though the scheduled task never actually ran. This is a real,
previously-unidentified correctness concern, not merely a missing
feature.

## Why implementing the callback still does not resolve this

Given `PPC_LOOKUP_FUNC` now makes *invoking* the guest routine
mechanically straightforward (mirroring `ExCreateThread`), the remaining
obstacle is not "can this project call back into guest code" — it can.
It is what the routine at `0x823917f8` actually *does* once invoked: it
is the task body for this same save-content-scan/write operation
(`Function_82391A40`), which reads and writes real save-file content on
media this project has deliberately kept read-only throughout (r189,
r199, r202, r229 all confirm this convention). Invoking the real routine
correctly would immediately hit the same deferred save/reload write-path
wall every other import in this cluster already hits — `NtWriteFile`/
`NtDeviceIoControlFile` remain unimplemented by explicit, standing
project decision (r202). Calling the routine without that write support
in place would not produce correct behavior; it would just move the
missing-feature boundary one level deeper without resolving it.

## Consequence

`XamTaskSchedule` is confirmed to be part of the same deferred
save/reload frontier as `NtWriteFile`/`NtDeviceIoControlFile`, not a
separate, independently-schedulable subsystem decision as r198's
original framing suggested. r230's split of `XamTaskCloseHandle` from
this pair stands correct and unaffected. The false-success-signal risk
identified this cycle is real but only actually reachable through the
same gated, currently-unexercised save-content-scan code path
(`Function_82391A40`) r229 already traced — it does not change Gate 2's
own reachability picture, and fixing it in isolation (without real write
support) would require either building that write support anyway, or a
narrower, still-somewhat-speculative patch (e.g., forcing the global to
a known "not completed" state) that this project's evidence discipline
would not currently support without further tracing of what other code
reads that global and how.

## Next

1. If the save/reload write-path decision is ever taken up (r202's
   named next step: `NtOpenFile`→`NtDeviceIoControlFile`→loop
   `NtWriteFile`/`NtSetInformationFile` in `Function_82392878`/the
   function at `0x82392978`), `XamTaskSchedule`'s real callback
   (`0x823917f8`, context `r11+0x6310`, options `0x02080002`) should be
   implemented alongside it using the `ExCreateThread`/`PPC_LOOKUP_FUNC`
   precedent (r114) — not before, since it cannot behave correctly
   without that write support regardless.
2. This does not reopen the offline-import sweep — `XamTaskSchedule`
   already had a named, correct disposition (blocked on save/reload);
   this cycle strengthens that disposition with concrete evidence rather
   than changing it.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
