# AC6 retail NTSC-U/J — real fix: `NtOpenFile` reuses `NtCreateFile`'s media-service path (r201)

Date: 2026-09-02.

## Qualification

Ghidra project `ghidra-projects/ac6-us.gpr`, `-readOnly -noanalysis`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No oracle
used. Real code change to
`recompilation/ace-combat-6-retail/tools/materialize_native_import_stubs.py`,
with a matching test. Verified via a clean `tools/build.py --target ntsc-uj
--profile native` (`ctest` 10/10) and the full retail-native pytest suite
(188/188, up from 187/187).

## What was found

`NtOpenFile` is by far the highest call-site-count import still on the
generic offline no-op sweep found so far: **9 real call sites**
(`0x821f7308`, `0x82390984`, `0x821f56ec`, `0x821f7440`, `0x82391388`,
`0x823929d0`, `0x82392908`, `0x82392424`, `0x82392dcc`).

Traced one directly (`0x821f7308`, inside `Function_821F7220`): the real
6-arg signature `NTSTATUS NtOpenFile(PHANDLE FileHandle, ACCESS_MASK
DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK
IoStatusBlock, ULONG ShareAccess, ULONG OpenOptions)` puts
`FileHandle`/`ObjectAttributes`/`IoStatusBlock` at exactly `r3`/`r5`/`r6`
— the identical register positions r122/r123/r129 already confirmed for
`NtCreateFile`'s own 9-arg signature. This project's media service is
read-only (r189/r190/r197/r199): "open" is the only real operation either
import performs, so the identical register contract is not a coincidence
worth treating separately.

## Fix

`NtOpenFile` is added to `NtCreateFile`'s existing `render_body` case
(`if name in {"NtCreateFile", "NtOpenFile"}:`), reusing the exact same
`ObjectAttributes`→`ANSI_STRING`→`guest_path_to_relative`→
`native_guest_media_service().open_file()` path r122/r123/r129 already
derived and traced. The one incidental cleanup: the shared body's debug
trace line was a literal `"[NtCreateFile]"` string; since the body is now
generated for two different import names, it is now an f-string
interpolating the real `name` (`"[NtCreateFile]"` / `"[NtOpenFile]"`),
so `AC6_NATIVE_IMPORT_TRACE` output correctly labels which import a given
line came from instead of mislabeling every `NtOpenFile` call as
`NtCreateFile`.

## Gates

`ctest` 10/10 (native profile, unchanged target count — no `native/`
source file touched this cycle, only the generated-stub tool, so no
`tools/prepare.py` re-run was needed). Full retail-native pytest suite:
188/188 (187/187 before this cycle, +1 new test,
`test_nt_open_file_reuses_the_create_file_media_service_path`; the
pre-existing `NtCreateFile` test still passes unchanged, confirming the
trace-label parameterization didn't regress its own output). `git status`
unchanged apart from the intended change set and the same pre-existing,
unrelated dirty state noted by every prior cycle in this series.

`tools/audit_ac6_mission01_native_gate.py` still fails on
`playable_session evidence size mismatch:
reconstruction/ace-combat-6/src/retail_session.cpp` — same pre-existing,
unrelated condition already named in r191-r200's reports, not touched by
this cycle.

## Next

1. Continue the broader offline-import sweep (same method as
   r90/r93/r164/r176-r201). `NtOpenFile`'s other 8 real call sites were
   not traced individually — the register-contract match with
   `NtCreateFile` was confirmed once and is structural (same ABI
   position), not per-call-site, so this is not expected to need
   revisiting unless a specific call site is later found to deviate.
2. Both of Gate 2's named frontiers remain unchanged: DATA.TBL chain fully
   traced and closed; `IM_LOAD_IMMEDIATE`→SPIR-V policy-blocked.
