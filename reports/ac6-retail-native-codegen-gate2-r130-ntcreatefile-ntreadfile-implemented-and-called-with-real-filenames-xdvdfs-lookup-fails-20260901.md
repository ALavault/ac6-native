# AC6 retail NTSC-U/J — `NtCreateFile`/`NtReadFile` implemented against real media; the guest now calls them with the exact real filenames, but `read_xdvdfs_file` can't find files that verifiably exist at the XDVDFS root (r130)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. **Native code changed for real this cycle** (not
reverted): two new files
(`native/include/ac6/native_guest_media.h`, `native/src/native_guest_media.cpp`),
`NtCreateFile`/`NtReadFile` implemented in
`tools/materialize_native_import_stubs.py`, `native/CMakeLists.txt`
and `native/include/ac6/native_runtime.h`/`native/src/native_runtime.cpp`
updated to wire the new service in. One round of `AC6_R130_DIAG`-gated
instrumentation was added to and fully reverted from generated
(gitignored) `ppc_recomp.22.cpp` for live verification; confirmed via
`grep -c "r130"` returning 0 and `ctest` 9/9 passing afterward.

## Implementing r129's named next step

r129 established the fix's shape (`NtCreateFile` -> `read_xdvdfs_file`,
`NtReadFile` -> real bytes) and that real content genuinely exists on
this project's own already-qualified retail ISO. This cycle
implemented it.

## New infrastructure: `NativeGuestMediaService`

Following the existing `native_guest_vd_service()` singleton pattern
(`native_guest_vd.h`/`.cpp`), added `native_guest_media_service()`:
`bind(MediaInput)` (called once from `NativeRuntime::boot()`, the same
`MediaInput` every other boot-time read already uses), `open_file`
(ISO mode via the existing `read_xdvdfs_file`; assets-directory mode
via a direct `std::filesystem` read; caches the full file in memory,
keyed by a monotonic handle), and `read_file` (copies bytes at an
offset into a raw host destination pointer). Registered in
`native/CMakeLists.txt`'s `ac6_native_xenos` target, which
`ac6recomp` already links.

## `NtCreateFile`/`NtReadFile`

Implemented against the calling convention and `OBJECT_ATTRIBUTES`
layout r122/r123 already confirmed byte-for-byte from real
disassembly and real bytes (not re-derived this cycle): reads
`ObjectAttributes.ObjectName` -> `ANSI_STRING{Length, MaximumLength,
Buffer}` -> the guest path string, strips a drive-letter-style prefix
(`guest_path_to_relative`, generic for any `X:\...` form, not
hardcoded to `game:`), opens via the media service, and returns a real
handle with `STATUS_SUCCESS` or `STATUS_OBJECT_NAME_NOT_FOUND` (a real
NT constant, not the generic `kOfflineStatus` sentinel). `NtReadFile`
completes synchronously with real `STATUS_SUCCESS`/`STATUS_END_OF_FILE`
-- deliberately never `STATUS_PENDING`, per r125/r126's own named
constraint: an unconditional pending stub would trade this crash for
an infinite busy-loop, since the caller's retry counter (r119) only
decrements on a *recognized* failure, never on pending, and this
harness has no real DMA to model asynchronously anyway.

Two real bugs were caught and fixed within this same cycle before they
reached a commit: a Python string-escaping mistake produced `'\'`
(an unterminated C++ char literal) instead of `'\\'` in the generated
backslash-to-slash normalization, and a second instance produced
unescaped quotes inside a generated `fprintf` format string. Both were
caught by the build failing immediately (not by a later, harder-to-
diagnose runtime symptom) and fixed before any commit. Also added a
permanent `AC6_NATIVE_IMPORT_TRACE`-gated trace line to `NtCreateFile`
(matching the existing `DbgPrint`/`ExCreateThread` convention) --
genuinely useful ongoing diagnostic infrastructure, not a one-off.

Four new/updated Python tests (`test_create_file_reads_object_attributes_and_opens_via_media_service`,
`test_read_file_completes_synchronously_not_pending_forever` --
explicitly asserting `STATUS_PENDING`'s value never appears in the
generated body, guarding the r125/r126 constraint --
`test_guest_path_to_relative_strips_drive_prefix`, plus one existing
test updated for the new trace line). Full suite: 26/26 in this file
(was 23/23), 139/139 across the retail-native pytest tree (was 136/136).

## Verified live against the real qualified ISO -- for the first time this entire investigation arc

Every prior cycle (r105-r129) probed `build/.../assets/`, containing
only `default.xex` (r129's own finding). This cycle ran the probe
against the actual qualified retail ISO
(`Ace Combat 6 - Fires of Liberation (USA, Japan) (En,Fr,De,Es,It).iso`,
hash-verified again this cycle against the workspace-root file) for
the first time. Real, measurable new behavior appeared: previously
unreached imports now fire (`XamLoaderLaunchTitle`,
`XamShowDirtyDiscErrorUI`, `VdGetSystemCommandBuffer`), and
`RtlNtStatusToDosError` now converts real NT statuses instead of only
ever seeing `kOfflineStatus`.

`NtCreateFile` is confirmed live to be called with exactly the
filenames r129 predicted:

```
[NtCreateFile] "DATA.TBL" -> not found
[NtCreateFile] "DATA00.PAC" -> not found
[NtCreateFile] "DATA01.PAC" -> not found
```

**All three report "not found"**, even though a raw byte scan of the
ISO this cycle performed shows these exact filenames sitting in a
directory listing immediately alongside `bgmpack.bin` and
`default.xex` (root-level entries, not nested in a subdirectory --
ruling out a path-prefix explanation). `read_xdvdfs_file`
(`native/src/native_xdvdfs.cpp`, a pre-existing, untouched-this-cycle
component with its own case-insensitive comparison already in place)
fails to find files this cycle independently confirmed exist at the
XDVDFS root. The crash is unchanged (still `sub_821D6C20`, same
deterministic site as every cycle since r116) -- expected, since the
game still receives no real file data.

## Decision

This is real, tested, independently-justified progress kept in the
tree: the entire `NtCreateFile`/`NtReadFile`/media-service stack is
correct in shape (verified by the trace showing exactly the expected
filenames reaching it) and does not regress anything (139/139,
`ctest` 9/9). It does not yet close Gate 2's crash, because the actual
byte read still fails one layer further down, in a pre-existing
component this cycle did not modify or debug. That is a distinct,
narrower, well-scoped remaining gap -- not a flaw in this cycle's own
new code -- and per this project's own discipline against rushing an
unverified fix into an unrelated component, it is named as the next
step rather than guessed at now.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. `git status` (run after `ctest`, not before):
no regenerated metrics artefact came back modified-but-unstaged.
`git status --porcelain=v1 -- recompilation/ace-combat-6-demo | wc -l`:
185, unchanged. Python suite: 139/139 (was 136/136).

## Next

1. Debug `read_xdvdfs_file` (`native/src/native_xdvdfs.cpp`) directly:
   why does it report "not found" for `DATA00.PAC`/`DATA01.PAC`/
   `DATA.TBL`, which this cycle confirmed by raw byte inspection sit
   at the XDVDFS root alongside `default.xex` (a file this exact
   reader already successfully finds and loads at every boot, per
   `NativeRuntime::boot()`'s own existing ISO-mode code path)? The
   difference between a file this reader already handles correctly
   (`default.xex`) and one it does not (`DATA00.PAC`) is the concrete
   next question -- check directory-entry parsing, 8.3-name handling,
   or sector/size-field edge cases specific to these larger files
   before assuming the bug's shape.
2. Once fixed, re-run the probe against the qualified ISO (already
   symlinked at a space-free path this cycle for reliable `gdb`
   invocation, `/fastdata/tmp/claude-1007/ac6_retail.iso` -- session-
   local, not committed) and verify live whether `0x82935d98` finally
   gets written and the original r100 crash stops.
3. This cycle's own new `AC6_NATIVE_IMPORT_TRACE`-gated `NtCreateFile`
   trace line remains available for that verification without needing
   new instrumentation.
