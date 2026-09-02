# AC6 retail NTSC-U/J — `XexCheckExecutablePrivilege` checked, no safe fix identified; native input backend needs a scoping decision (r178)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle — a static-only check plus
a scoping note, no diagnostic added or reverted, no build touched.

## `XexCheckExecutablePrivilege` — checked, not fixed

Continuing the broader offline-import sweep after r177, this real call
(3 real call sites: `0x821f5d08`, `0x82391ac8`, `0x82391d74`) is checked
with two *different* privilege IDs (`0xa` and `0x17`), both tested as
`cmplwi r3,0x0` booleans. `kOfflineStatus` (`0xC00000BB`) is nonzero, so
every examined site currently evaluates as "privilege granted" —
coincidentally non-breaking, the same category r164 named for
`RtlTryEnterCriticalSection`. Unlike `XamUserGetSigninState`/
`XamGetSystemVersion` (r176/r177), no call site's own logic pins which
answer (granted/denied) is real-hardware-correct for these two specific
IDs, and this project has no reachable, non-guessed source for privilege-ID
semantics. Guessing "denied" risks *introducing* a new bail path that does
not currently execute; guessing "granted" (matching current coincidental
behavior) would be cosmetic-only. Following r164's own precedent, this is
named, not fixed, absent stronger evidence.

## Native controller input — scoping note, not started

`XamInputGetState`/`XamInputSetState`/`XamInputGetCapabilities` remain the
strongest candidate for this project's own historically observed symptom
("`XPSO-164` atteint gameplay, contrôles nuls", STATE.md). Checking
`recompilation/ace-combat-6-retail/native/CMakeLists.txt` confirms **no
host input library is linked** (only `OpenSSL::Crypto`) — implementing
real controller I/O is not a single-import contract-shape fix like r169-
r177; it requires:

- choosing and adding a new host input dependency (e.g. SDL2, already
  implied elsewhere in this project's own Xvfb/`SDL_AUDIODRIVER=dummy`
  conventions, but not currently linked into the `native/` product tree);
- deriving this XEX's own real `XINPUT_STATE`-equivalent struct layout
  from further Ghidra work (the trampoline at `0x82390ce8` tail-jumps
  straight into the import with no visible field consumer in this cycle's
  own reading — the real consumer is further away and not yet traced);
- new source files and a new dependency in the build, not a generated-stub
  edit.

This is a materially larger, architecturally significant change (new
dependency choice, new subsystem) rather than a bounded correctness fix,
and this project's own discipline is to not invent or expand work beyond
what is asked without naming the decision explicitly. Not started this
cycle; named for an explicit go/no-go before any implementation begins.

## Gates

No native code changed, no build touched. `ctest`'s last-known state (9/9,
r177) stands unaffected. `git status` unchanged apart from pre-existing,
unrelated dirty state.

## Next

1. A go/no-go decision on the native controller input backend (library
   choice, scope) is needed before implementation starts.
2. Absent that decision, continue the broader offline-import sweep (same
   method as r90/r93/r164/r176/r177) for further contract-shape or
   struct-fill candidates not requiring new infrastructure.
3. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
