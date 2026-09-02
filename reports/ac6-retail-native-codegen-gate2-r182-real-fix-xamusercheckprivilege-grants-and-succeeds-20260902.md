# AC6 retail NTSC-U/J — real fix: `XamUserCheckPrivilege` grants and succeeds (r182)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(166/166, up from 165/165).

## Why this check was worth running

Continuing the offline-import sweep. `0x823cfe8c`, the target of a `bl`
r176's own report named only as "a different function entirely (a
sign-in-prompt/fallback path)" without identifying it, turned out to be
`XamUserCheckPrivilege`'s own thunk — confirmed by matching this cycle's
symbol lookup against that exact address. With three real call sites, it
was worth checking directly rather than left unnamed.

## What was found

Real contract: `DWORD XamUserCheckPrivilege(DWORD dwUserIndex, DWORD
dwPrivilegeType, LPBOOL pfResult)` — a struct-fill (the bool) **plus** a
real status return, not a discarded status. This XEX's own real call sites
consume both halves:

**`0x82206bcc`/`0x82206bf0`** (a restriction-flag gate):

```
82206bc0  addi r5,r1,0x60
82206bc4  lwz r3,0x0(r31)
82206bc8  li r4,0xfc
82206bcc  bl 0x823cfe8c        ; XamUserCheckPrivilege(user, 0xFC, &bool)
82206bd0  cmplwi r3,0x0
82206bd4  bne 0x82206c18       ; call itself failed -> treat as restricted
82206bd8  lwz r11,0x60(r1)
82206bdc  cmpwi cr6,r11,0x1
82206be0  beq cr6,0x82206c18   ; privilege GRANTED -> same target as failure
```

Only "the call succeeds AND the privilege is denied" takes the other path
(checking a second privilege, `0xFB`). `ERROR_SUCCESS` (`0`) is the real
success code this checks the return against directly.

**`0x821f44ac`** (r176's own sign-in-resolution helper): returns this
call's raw result as its **own** return value with no further check —
`kOfflineStatus` (`0xC00000BB`) was a nonsensical NT status handed straight
to that function's caller in place of a real Win32 error code.

## Value chosen

`ERROR_SUCCESS` (`0`), with the output bool set to `TRUE` (privilege
granted). Consistent with this project's own established single,
unrestricted offline-profile assumption (r176's "signed in locally, not to
Live" reasoning) — not a value picked to force the one call site where the
outcome actually differs (that site only diverges on "succeeds AND
denied", and this project has no reachable evidence pinning what
`0xFC`/`0xFB`/the other observed IDs (`0xA`, `0x17`) specifically restrict,
matching r178's own caution about guessing privilege-ID semantics).

## Gates

`ctest` 10/10 (native profile). Full retail-native pytest suite: 166/166
(165/165 before this cycle, +1 new test). `git status` unchanged apart from
the intended change set and pre-existing, unrelated dirty state.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176/r177/r179/r181/r182).
2. `XexCheckExecutablePrivilege` (r178, a *different* import — `DWORD
   XexCheckExecutablePrivilege(DWORD)`, no output pointer) remains named,
   not fixed; unlike `XamUserCheckPrivilege`, its own call sites' return-
   value semantics did not pin a safe direction.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
