# AC6 retail NTSC-U/J — documentation only: real save-write path traced (`NtWriteFile`/`NtDeviceIoControlFile`); SEH and console-key imports checked, no safe fix (r202)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. **No code change this cycle** — documentation only, same class as
r164/r178. `ctest`/pytest unaffected: 188/188 pytest, 10/10 ctest (last
verified at r201, unchanged since no source was touched).

## What was found: `NtWriteFile`/`NtDeviceIoControlFile` are a real save-write path

`NtWriteFile` has 8 real call sites (`0x821f5578`, `0x821f55c4`,
`0x82391060`, `0x82392bc8`, `0x82392c34`, `0x82392ca0`, `0x82392d30`,
`0x8239225c`). `NtDeviceIoControlFile` has 3 (`0x82392948`, `0x82392a0c`,
`0x82392a80`) — all three inside the same function neighborhood as four
of the `NtWriteFile` sites.

Tracing that neighborhood (`Function_82392878` and the function starting
`0x82392978`, both adjacent to r199's `NtFlushBuffersFile` fix at
`0x82392838`): the real code opens a file via `NtOpenFile`, queries
device/volume geometry via `NtDeviceIoControlFile` (an IOCTL call whose
control code was not decoded this cycle), then writes a buffer in a loop
of `NtWriteFile` calls with an explicitly advancing byte offset
(`ld r11,0x68(r1); ...; add r11,r11,r10; std r11,0x68(r1)` repeating
across each iteration) sized against the queried device geometry. This is
unmistakably a **real save-file writer** — chunked, offset-advancing,
device-geometry-aware writes are not a debug/telemetry pattern, they are
exactly what a FATX-aware save writer looks like.

This is the concrete binary-level shape of the "save/reload" frontier
`NEXT.md` already names as blocked by Gate 2. It was not implemented this
cycle: a correct fix needs real write support added to
`NativeGuestMediaService` (currently read-only, per r189/r190/r197/r199),
plus decoding the specific IOCTL control code(s) this XEX's
`NtDeviceIoControlFile` calls actually request — both larger, scoped
efforts in their own right, not a one-line contract fix. Named here so a
future save/reload cycle has the real call-site addresses already
identified rather than starting the trace from nothing.

## Also checked, no safe fix: SEH primitives

`RtlRaiseException` (3 real call sites), `RtlUnwind` (3 real call sites),
and `RtlCaptureContext` (1 real call site, `0x8238f388`) are the core of
Win32 structured exception handling — raising an exception, unwinding the
stack through registered handlers, and capturing a full register-state
`CONTEXT` record respectively. This project has no SEH dispatch
infrastructure (no `__C_specific_handler` implementation either — that
import has 0 real call sites XEX-wide, so it is not itself in play, but
its absence confirms no SEH engine exists here). Implementing any one of
these three correctly, in isolation, would either be inert (nothing to
unwind to) or require building the real dispatch/unwind engine all three
depend on together — not a bounded, one-import fix. Left as the generic
offline no-op.

## Also checked, permanently out of scope: `XeKeysConsolePrivateKeySign`

Single real call site (`0x82390ff4`). Real semantics: signs data with
this specific console's own hardware-burned private key. This is not
something derivable from the XEX or any static analysis this project can
perform — it is a genuine per-console hardware secret. Unlike the
SEH/write-support items above, this is not "needs more scoped effort
later," it is **permanently** out of reach for this project's own
no-oracle, no-external-secret discipline. `XeKeysConsoleSignatureVerification`
(1 real call site) is the verification counterpart and shares the same
limit.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r201) for a bounded fix candidate.
2. If a save/reload cycle is ever undertaken, start from the exact
   addresses named above (`NtOpenFile`→`NtDeviceIoControlFile`→
   `NtWriteFile` loop in `Function_82392878`/the function at
   `0x82392978`) rather than re-discovering the shape from scratch.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
