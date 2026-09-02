# AC6 retail NTSC-U/J — real fix: `XamUserGetSigninInfo` fills the confirmed gating bit for user 0 (r227)

Date: 2026-09-03.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. `ctest` 10/10 (`build/ntsc-uj/native/native-cmake`), pytest
207/207 (206 passed, 1 pre-existing skip).

## What was found

r211 verified `XamUserGetSigninInfo` without confirming a fix ("return
-masking pattern not confirmed"). This cycle re-traced it with the same
`scripts/ReferencesTo.java` technique carried forward from r225/r226.

`scripts/FindSymbolReferences.java` found one direct reference to the
import label: `0x821f519c UNCONDITIONAL_CALL`, inside a small wrapper
function `Function_821F5190`. Its raw disassembly (`scripts/DumpRange.java`)
shows the prologue (`mfspr`/`stw`/`stwu`) followed immediately by `bl
0x823cff9c` with no intervening register writes — confirming it is a pure
passthrough that forwards its own `r3`/`r4`/`r5` unchanged to the real
import, not a remapping wrapper.

`scripts/ReferencesTo.java` against the wrapper's own address
(`0x821f5190`) found six real callers. Three were decompiled
(`Function_821CF008`/`0x821cf060`, `Function_821B65D0`/`0x821b66bc`,
`Function_821CE6C0`/`0x821ce6f0`). All three call the wrapper with the same
three-argument shape: `(dwUserIndex, 0, &local_buffer)`, where
`local_buffer` is a contiguous 12-byte stack region (an 8-byte slot
immediately followed by a 4-byte slot in each caller's own local layout).
This matches the documented
`XamUserGetSigninInfo(DWORD dwUserIndex, DWORD dwFlags, PXUSER_SIGNIN_INFO
pSigninInfo)` contract in shape (three arguments, an output struct), used
here only to check the argument *count* and the buffer's own confirmed
byte layout against these three independent real call sites — not copied
as a value from any external source.

Every one of the three callers reads exactly one field back: a single bit
at buffer offset `+8` (`(field >> 1) & 1`), and gates real per-player
initialization logic on it — when the bit is set, all three callers skip
their real-profile logic entirely (an early return or a bypassed block);
when clear, real logic (persistent profile flag reads, achievement/data
initialization) runs. The generic offline default never writes this
buffer at all, so this bit was always read from stale, previously
uninitialized stack bytes — the same "uninitialized read gates real
control flow" risk class r226 (`XamUserGetName`) and r183
(`RtlImageXexHeaderField`) already established, except here the outcome is
whether real per-player state gets initialized at all, not just a display
string.

Offsets `+0..+7` (the XUID-sized leading field) are never read by any of
the three traced callers — nothing was asserted for them beyond zero,
consistent with this project's "don't write what wasn't read" discipline
applied elsewhere (r108's `MmQueryStatistics`).

## Fix

`tools/materialize_native_import_stubs.py`: `XamUserGetSigninInfo` now
mirrors this file's own existing `XamUserGetSigninState` (r176) convention
— user index 0 is the offline session's one signed-in-locally user, every
other index is not signed in. For user 0: writes zero across offsets
`+0..+11` (XUID zero — no real Xbox Live identity exists offline; the
gating bit at `+8` clear, so every traced caller's real-profile path runs
instead of its skip path) and returns `STATUS_SUCCESS`. For any other
index: keeps this file's prior offline failure (`kOfflineStatus`) via
`trace_offline_import`, unchanged from before this cycle.

## Gates

- `python3 -m pytest tests/` — 207/207 (206 passed, 1 pre-existing skip;
  new test
  `test_xam_user_get_signin_info_fills_the_confirmed_fields_for_user_zero`).
- `python3 tools/build.py --target ntsc-uj --profile native` — clean,
  stub object rebuilt (confirmed newer than the regenerated source).
- `ctest` (`build/ntsc-uj/native/native-cmake`) — 10/10.
- `git status` after `ctest` (not before): only the two edited source files
  changed.

## Next

1. Continue the offline-import sweep per r218's bucket catalog — 83
   generic imports remain after this fix.
2. `XamUserGetXUID` (r211, verified not fixed) and `XamWriteGamerTile`,
   the `XamContent*` cluster (`CreateEnumerator`/`Close`/`Delete`/
   `GetDeviceData`/`GetDeviceState`/`SetThumbnail`, save/reload-adjacent
   territory per r202/r212) remain unexamined candidates.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
