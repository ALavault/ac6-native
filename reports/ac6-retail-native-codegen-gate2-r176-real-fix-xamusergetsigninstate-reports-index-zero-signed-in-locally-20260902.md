# AC6 retail NTSC-U/J — real fix: `XamUserGetSigninState` reports index 0 signed in locally (r176)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 9/9) and the full retail-native pytest suite
(159/159, up from 158/158).

## Why this check was worth running

The Vd/X platform-config family (r168-r175) is closed with no further named
gap. This cycle is a fresh, broader static sweep of the 151 imports still on
the generic offline-import fallback (the method r90/r93/r164 used), looking
for real call sites where a wrong-shaped return controls real flow —
following on from this project's earlier observed symptom "`XPSO-164`
atteint gameplay, contrôles nuls" (STATE.md), which pointed at controller/
sign-in-adjacent imports as a plausible area to check next.

## What was found

`XamUserGetSigninState`'s real contract is `DWORD
XamUserGetSigninState(DWORD dwUserIndex)`, a real enum (`0` = not signed
in, `1` = signed in locally, `2` = signed in to Xbox Live), not a status.
Its real call sites gate real control flow on the exact result, not a
discarded read:

**`0x821f4428`** (a sign-in resolution helper): loops user indices `0..3`
(or checks one specific index passed in) calling this import and testing
for exact equality against `1`:

```
821f4460  or r3,r31,r31
821f4464  bl 0x823cfe6c        ; XamUserGetSigninState(r31) -> r3
821f4468  cmpwi cr6,r3,0x1
821f446c  beq cr6,0x821f4480   ; found the active signed-in user
821f4470  addi r31,r31,0x1
821f4474  cmplwi cr6,r31,0x4
821f4478  blt cr6,0x821f4460   ; else try the next index
```

The first index whose result is **exactly** `1` is treated as the active
signed-in user (stores a success code and proceeds); if no index matches,
execution falls through to a *different* function entirely (a sign-in-
prompt/fallback path). `kOfflineStatus` (`0xC00000BB`) never equals `1` for
any index, so this always fell through to that fallback — a real,
consequential bug, not a cosmetic one, and a plausible contributor to a
menu/flow that never reaches gameplay for lack of a "signed-in" user.

**`0x82206954`** (an unrelated per-player update function): tests the
result against `0`:

```
82206950  lwz r3,0x0(r31)
82206954  bl 0x823cfe6c        ; XamUserGetSigninState(r3) -> r3
...
8220697c  cmpwi cr6,r29,0x0
82206980  beq cr6,0x82206aa4   ; not signed in -> skip this update
```

`0` gating a skip is consistent with real hardware's own "not signed in"
sentinel — the same enum, independently corroborated.

## Value chosen

Index `0` → `1` (signed in locally); every other index → `0` (not signed
in). This project's own established convention throughout this file is
single-player, fully offline (every stub is commented "Offline-only HLE
boundary; no socket or host I/O side effect"), so index 0 is signed in
**locally** (`1`) — not to Xbox Live (`2`), which would assert real
network/account state this project has never modeled.

## Gates

`ctest` 9/9 (native profile). Full retail-native pytest suite: 159/159
(158/158 before this cycle, +1 new test). `git status` unchanged apart from
the intended change set and pre-existing, unrelated dirty state.

## Next

1. The same sign-in-resolution helper (`0x821f4428`) also reads
   `XamGetSystemVersion` first and gates on a version threshold
   (`0x20096b00`) before ever reaching the signin-state loop for the
   `dwUserIndex == 0xff` (any-user) path; `XamGetSystemVersion` is itself
   still on the generic offline-import fallback and not yet checked for
   the same category of gap — a natural next candidate.
2. `XamInputGetState`/`XamInputSetState`/`XamInputGetCapabilities` (real
   controller I/O) remain unimplemented; this project's own history
   ("`XPSO-164` atteint gameplay, contrôles nuls") suggests dead controls
   may trace here, but implementing them requires a real native input
   backend (none exists yet under `native/`), a materially larger task
   than this cycle's single-import contract-shape fix — named, not
   undertaken here.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
