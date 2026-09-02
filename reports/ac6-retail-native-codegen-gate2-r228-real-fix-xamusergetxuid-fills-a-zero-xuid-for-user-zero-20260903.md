# AC6 retail NTSC-U/J — real fix: `XamUserGetXUID` fills a zero XUID for user 0 (r228)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. `ctest` 10/10 (`build/ntsc-uj/native/native-cmake`), pytest
208/208 (207 passed, 1 pre-existing skip).

## What was found

r211 verified `XamUserGetXUID` without confirming a fix. This cycle
continued the same wrapper-tracing technique used for r227.

`scripts/FindSymbolReferences.java` found one direct reference to the
import label: `0x821f462c UNCONDITIONAL_CALL`, inside a wrapper function
`Function_821F4618`. Its raw disassembly (`scripts/DumpRange.java`) shows,
between the prologue and the `bl`:

```
821f4624 or  r5,r4,r4
821f4628 li  r4,0x7
821f462c bl  0x823cfedc
```

This is an argument-remapping wrapper, not a pure passthrough: it takes
`(dwUserIndex, pXuid)` and inserts a literal `dwFlags=7` between them
before calling the real import with the confirmed three-argument shape
`XamUserGetXUID(DWORD dwUserIndex, DWORD dwFlags, PXUID pXuid)` — the same
"wrapper remaps a reduced argument list onto the real signature" pattern
already resolved for `NtSetTimerEx` (r216) and `XamShowMessageBoxUIEx`
(r221/r222).

`scripts/ReferencesTo.java` against the wrapper's address (`0x821f4618`)
found six real callers; three were decompiled and confirm an 8-byte XUID
output buffer:

- `Function_821CFCE0` loops `dwUserIndex` 0..3, calling the wrapper for
  each and comparing the retrieved 8-byte XUID against a caller-supplied
  one, to find which local user index owns a given XUID.
- `Function_821CFDD8` returns the 8-byte value directly as its own
  function return, gated on the wrapper's own status being 0.
- `Function_821CE9A0` copies the 8-byte value unconditionally into a
  struct field with **no status check at all** — the same
  "buffer read regardless of status" risk class already established for
  `XamUserGetName` (r226) and `XamUserGetSigninInfo` (r227). Under the
  prior generic offline default, which never wrote this buffer, that copy
  was always reading uninitialized stack bytes as though they were a real
  XUID.

## Fix

`tools/materialize_native_import_stubs.py`: `XamUserGetXUID` now mirrors
r227's convention — user index 0 (this file's one signed-in-locally user,
per `XamUserGetSigninState`, r176) gets 8 bytes of zero written to the
confirmed XUID output buffer (`ctx.r5.u32`) and `STATUS_SUCCESS`; no real
Xbox Live XUID exists offline, matching the same zero-XUID convention r227
already used for the leading field of `XUSER_SIGNIN_INFO`. Other user
indices keep the prior offline failure. The wrapper's own `dwFlags=7`
argument is not read by the stub — nothing in the traced evidence shows
flag-dependent behavior, and this file's own `XamUserGetSigninInfo` fix
similarly ignores its `dwFlags` argument.

## Gates

- `python3 -m pytest tests/` — 208/208 (207 passed, 1 pre-existing skip;
  new test `test_xam_user_get_xuid_fills_a_zero_xuid_for_user_zero`).
- `python3 tools/build.py --target ntsc-uj --profile native` — clean.
- `ctest` (`build/ntsc-uj/native/native-cmake`) — 10/10.
- `git status` after `ctest` (not before): only the two edited source files
  changed.

## Next

1. Continue the offline-import sweep per r218's bucket catalog — 82
   generic imports remain after this fix.
2. `XamWriteGamerTile` and the `XamContent*` cluster
   (`CreateEnumerator`/`Close`/`Delete`/`GetDeviceData`/`GetDeviceState`/
   `SetThumbnail`, save/reload-adjacent territory per r202/r212) remain
   unexamined candidates.
3. `Function_821F4618` (this cycle's wrapper) happens to share its base
   address with the region r225 described as bracketing the closed
   dispatch-table trampoline cluster (`symbol-before`/`symbol-after` for
   those addresses). This is an unrelated coincidence of unlabeled code
   layout in this XEX, not a reopening of that closed thread — noted so a
   future cycle doesn't conflate the two.
4. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
