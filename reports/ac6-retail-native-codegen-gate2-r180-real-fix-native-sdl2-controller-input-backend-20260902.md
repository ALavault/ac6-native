# AC6 retail NTSC-U/J — real fix: native SDL2 controller input backend (r180)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`, extracted
freshly from the qualified ISO (`204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`)
via `tools/extract_xdvdfs_file.py` after `game-files/default.xex` turned out
to be the demo XEX, not this retail target's (confirmed by SHA-256
mismatch before any use). No oracle used. Real code + new source files,
with matching tests. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10, up from 9/9 — a new test target) and the
full retail-native pytest suite (164/164, up from 163/163). User
explicitly authorized SDL2 for this backend.

## Environment note: `native/` source changes need `prepare.py`, not just `build.py`

Every prior cycle in this campaign (r148-r179) only ever edited
`tools/materialize_native_import_stubs.py`, which `build.py` regenerates
fresh on every invocation. This is the first cycle to change `native/`
source files themselves (new `.cpp`/`.h`, `CMakeLists.txt`). `build.py`
compiles from `build/<target>/native/native-source/`, a **snapshot** copy
of `native/` made once by `tools/prepare.py --profile native` (`shutil.
copytree`) — it does not re-sync on every build. Editing `native/` without
re-running `prepare.py` silently builds the *old* snapshot. Re-running
`prepare.py --profile native` requires `--xex`/`--iso` only for hash
verification (recorded in the manifest); for the native profile it does
not copy retail assets or clone `AC6_recomp` (`materialize_native` in
`prepare.py`), so it is cheap and safe to re-run whenever `native/` itself
changes.

## What was built

`XamInputGetState`/`XamInputSetState`/`XamInputGetCapabilities` were
completely unimplemented (generic offline fallback, `kOfflineStatus`
always), the strongest candidate for this project's own historically
observed symptom ("`XPSO-164` atteint gameplay, contrôles nuls", STATE.md).

**New service** — `native/include/ac6/native_guest_input.h` /
`native/src/native_guest_input.cpp`: `NativeGuestInputService`, following
this codebase's own established singleton-accessor pattern
(`native_guest_vd_service()`, `native_guest_media_service()`). Wraps SDL2's
`SDL_GameController` API: `is_connected`, `get_state` (fills a
`GamepadState` from real polled hardware), `set_vibration` (best-effort
rumble). No controller physically present is treated as a normal, expected
result (`false`), never fabricated.

**Real evidence for the wire contract** — this XEX's own real call site at
`0x8234cedc` (`lwz r3,0x4(r31); addi r4,r31,0x44; bl <XamInputGetState>`)
confirms the `(dwUserIndex, &XINPUT_STATE)` argument shape, and its
immediate check `cmplwi cr6,r3,0x48f` confirms the real
`ERROR_DEVICE_NOT_CONNECTED` (`0x48F`) error code — read directly from
this XEX's own disassembly, not assumed. `0x82390d48`
(`XamInputGetCapabilities`) confirms `struct+0x1` (SubType) and
`struct+0x2` (Flags) byte-for-byte against Microsoft's own published
`XINPUT_CAPABILITIES` layout. The full `XINPUT_STATE`/`XINPUT_GAMEPAD`/
`XINPUT_VIBRATION` byte layouts used for the remaining, unread-at-this-
XEX's-call-sites fields are Microsoft's own fixed, cross-platform ABI
(identical on Windows and Xbox 360) — external protocol knowledge, the
same category as `XC_LANGUAGE_ENGLISH` (r174), not a guessed offset
specific to this XEX's own compiled layout.

**Wiring** — `tools/materialize_native_import_stubs.py` now generates real
bodies for all three imports, calling into `NativeGuestInputService` and
mapping SDL's axis/button conventions onto XInput's (Y-axis sign flip;
trigger `0..32767` → `0..255`; `SDL_GameControllerButton` → `XINPUT_GAMEPAD
wButtons` bits).

**Build** — `find_package(SDL2 REQUIRED)`, linked into `ac6_native_xenos`.
A real static-archive link-ordering bug surfaced and was fixed: nothing in
`ac6_native_xenos` itself referenced the new service (unlike Vd/media,
which are transitively reached from `NativeRuntime`), so GNU `ld` never
extracted `native_guest_input.cpp.o` from the archive before
`ac6_native_guest` (containing the generated stubs that *do* call it) was
processed — declaring `target_link_libraries(ac6_native_guest PRIVATE
ac6_native_xenos)` fixed the ordering. `tools/build.py`/`tools/validate.py`
had hardcoded `9`/`8`-binary and `9/9`/`8/8` ctest-count expectations for
the old test-target list; both updated to `10`/`9` and `10/10`/`9/9` for
the new `ac6_native_guest_input_tests` target.

**Test** — `native/tests/native_guest_input_tests.cpp`: this sandbox has no
real controller, so it asserts the honest, environment-agnostic contract
("not connected" for every user index, in-range or out-of-range) rather
than asserting fabricated hardware state.

## Gates

`ctest` 10/10 (native profile, including the new
`ac6_native_guest_input_tests`). Full retail-native pytest suite: 164/164
(163/163 before this cycle, +3 new tests). `tools/validate.py --target
ntsc-uj --runtime native` passes end-to-end with the updated counts.
`git status` unchanged apart from the intended change set and pre-existing,
unrelated dirty state.

## Next

1. Real controller input has not been exercised in this sandbox (no
   physical device); this closes the implementation gap but the
   "contrôles nuls" symptom itself needs a fresh runtime observation with
   a real controller to confirm the fix, not undertaken this cycle
   (no oracle/A/B without a named causal ambiguity, and no controller
   hardware is attached here).
2. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176/r177/r179) for further contract-shape or struct-fill
   candidates.
3. `XexCheckExecutablePrivilege` (r178) remains named, not fixed.
4. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
