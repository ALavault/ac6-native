# AC6 retail NTSC-U/J — real fix: `XamUserGetName` writes a synthetic name and succeeds (r226)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. `ctest` 10/10 (`build/ntsc-uj/native/native-cmake`), pytest
206/206 (205 passed, 1 pre-existing skip).

## What was found

Following up on r225's closed dispatch-table thread, this cycle returned to
r218's bucket catalog and picked `XamUserGetName` from the still-generic
import list (85 remaining as of the r225 checkpoint, regenerated fresh this
cycle from `build/ntsc-uj/native/codegen-20260831-mapfix-96838/`).

`scripts/FindSymbolReferences.java` found `XamUserGetName`'s import label
(`823cfe5c`) referenced once, by an `UNCONDITIONAL_JUMP` from a one
-instruction trampoline at `0x821f4410`. `scripts/ReferencesTo.java`
(carried forward from r225) against that trampoline address found two real
`UNCONDITIONAL_CALL` sites:

- `0x82161bb8`, inside `Function_82161B08`
- `0x821cfd98`, inside `Function_821CFD50`

Both were decompiled (`scripts/DecompileMany.java`). Both call
`func_0x821f4410` with three arguments matching the register convention
`(r3, r4, r5)` = `(dwUserIndex, szUserName, cchUserName)`, and **both pass
the literal constant `0x10`** as the third argument, independently
confirming the documented XAM contract
`XamUserGetName(DWORD dwUserIndex, LPSTR szUserName, DWORD cchUserName)`
with a 16-byte buffer — this is the well-known Xbox 360
`XUSER_NAME_SIZE` convention, used here only as a protocol-shape check
against two real, independent call sites in this XEX, not copied from an
external source as a value.

`Function_821CFD50` checks the return value (`== 0` on success) before
treating the buffer as valid, and zeroes it explicitly in its own fallback
branch — this caller is already safe against the generic
`kOfflineStatus`-only default. `Function_82161B08` does **not** check the
return value at all: it calls `func_0x821f4410(...)` and then immediately
uses the 16-byte region downstream regardless of outcome. Under the
previous generic offline stub, that buffer was never written by this call
at all, so whatever bytes previously occupied that stack/struct slot would
be treated as the gamertag unconditionally — the same uninitialized-read
risk class r183 (`RtlImageXexHeaderField`) already established for an
import whose caller doesn't check the status either.

## Fix

`tools/materialize_native_import_stubs.py`: `XamUserGetName` now writes a
short, explicitly-synthetic ASCII placeholder name (`"Player"` — no real
gamertag exists offline, and nothing in this build's evidence suggests one
should be fabricated to look authentic), null-terminated and bounds-checked
against the caller-supplied `cchUserName` (`ctx.r5.u32`), and returns
`STATUS_SUCCESS` (`ctx.r3.u64 = 0u`) unconditionally. Both real callers'
own success paths are now taken honestly instead of one of them reading
uninitialized memory.

## Gates

- `python3 -m pytest tests/` — 206/206 (205 passed, 1 pre-existing skip;
  new test `test_xam_user_get_name_writes_a_synthetic_name_and_succeeds`).
- `python3 tools/build.py --target ntsc-uj --profile native` — clean.
- `ctest` (`build/ntsc-uj/native/native-cmake`) — 10/10.
- `git status` after `ctest` (not before): only the two edited source files
  changed; no build artefact was regenerated without being staged.

## Next

1. Continue the offline-import sweep per r218's bucket catalog — 84
   generic imports remain after this fix.
2. `XamUserGetSigninInfo`/`XamUserGetXUID` (r211, verified not fixed — the
   return-masking pattern was not confirmed) and `XamWriteGamerTile`,
   `XamContentCreateEnumerator`/`Close`/`Delete`/`GetDeviceData`/
   `GetDeviceState`/`SetThumbnail` (save/reload-adjacent, r202/r212
   territory) remain unexamined candidates for a future cycle.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
