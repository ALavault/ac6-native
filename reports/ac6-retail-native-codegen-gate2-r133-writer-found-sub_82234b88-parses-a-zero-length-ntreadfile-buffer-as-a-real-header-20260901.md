# AC6 retail NTSC-U/J — the writer is found: `sub_82234B88` parses a `DATA.TBL` buffer that `NtReadFile` never filled (`length=0`), reading poison bytes as a real header field and computing a wild pointer into the notification list (r133)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (read-only, `-noanalysis`, no data
changed), XEX US `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
No oracle used. **No committed native source changed this cycle.** Three
rounds of throwaway, env-gated diagnostic instrumentation (`AC6_R133_DIAG`)
were added directly to gitignored, regenerated build-tree files
(`generated/ppc_context.h`, `generated/ppc_recomp.27.cpp`,
`generated/ppc_recomp.32.cpp`, `native-import-stubs.cpp`), used to take
live measurements, and fully reverted -- the three `generated/` files via
`cp` from `/tmp/*.orig` backups, `native-import-stubs.cpp` by re-running
`tools/build.py` (which regenerates it fresh from
`tools/materialize_native_import_stubs.py` every invocation). Confirmed
reverted via `grep -c "r133"` returning 0 in all four files, a full clean
`tools/build.py --target ntsc-uj --profile native` (`ctest` 9/9), and
139/139 Python tests. `git status` on `recompilation/ace-combat-6-retail`
shows only the pre-existing, unrelated `upstream/AC6_recomp` submodule
pointer change.

## Method: a universal store watch, not another guess at which function to instrument

r132 ruled out `NtReadFile` as the writer by tracing its one call directly,
but could not name the actual writer. Rather than keep guessing which
function to instrument next, this cycle added a **global watch** at the
lowest level shared by every generated store: `PPC_STORE_U8/U16/U32/U64` in
`generated/ppc_context.h` (four macros, one file, every `PPC_STORE_*` call
site in the whole codebase passes through them). Gated behind
`AC6_R133_DIAG`, each store whose target address falls in
`[0x823F0C30, 0x823F0C60)` -- the critical-section object and notification
list r131/r132 already mapped -- prints its address and value. This finds
the writer regardless of which of the (potentially many) functions in the
whole XEX performs it, without a repeat of r128's unreliable GDB
watchpoints.

## The writer: `sub_82234B88`, twelve sequential misaligned `u32` stores

The watch caught the exact corrupting sequence -- twelve `PPC_STORE_U32`
calls at addresses stepping by exactly 4 bytes, starting at `0x823F0C32`
(not 4-aligned: the store loop's own stride is correct, but its base is
computed from unrelated data, landing 2 bytes off any real field
boundary in the critical-section/list structures):

```
STORE_U32 addr=0x823f0c32 value=0x00000004
STORE_U32 addr=0x823f0c36 value=0x3f820000
STORE_U32 addr=0x823f0c3a value=0x3f82380c
STORE_U32 addr=0x823f0c3e value=0xffff380c
STORE_U32 addr=0x823f0c42 value=0x0000ffff
STORE_U32 addr=0x823f0c46 value=0x00000000
STORE_U32 addr=0x823f0c4a value=0x91820000
STORE_U32 addr=0x823f0c4e value=0x9182d85f
STORE_U32 addr=0x823f0c52 value=0x1000d85f
STORE_U32 addr=0x823f0c56 value=0x02000000
STORE_U32 addr=0x823f0c5a value=0x01000000
STORE_U32 addr=0x823f0c5e value=0x01000000
```

Bytes 0x823F0C4C-0x823F0C4F (from the overlapping stores at `0x4a` and
`0x4e`) end up `00 00 91 82` -- read back big-endian as `0x00009182`,
**exactly** the corrupted "head" value r131/r132 both observed. This is
not a coincidence needing further confirmation; it is the same four bytes.

`addr2line` against the built `ac6recomp` binary resolved the call stack
that reached this store to `_xstart -> sub_821D7DE0 -> sub_821D5F48 ->
sub_821CC508 -> sub_82234B88`. `sub_821D5F48`/`sub_821CC508` are the exact
retry-loop pair this whole investigation traced from r117 onward;
`sub_82234B88` is new to this cycle.

## What `sub_82234B88` actually does, read from its generated C++

`sub_82234B88(r3=dest, r4=src)` parses a record: it reads a 16-bit field
from `src+6` to compute a stride, writes bookkeeping fields into `*dest`
(offsets 0, 4, 8, 10-11, 12, 16, 24, 28), then runs a loop (`r10` counted
against `[dest+0]`) that in-place byte-swaps four parallel `u32` arrays
whose **base addresses are themselves fields of `*dest`**, at offsets 12,
16, 24, and 28 -- each computed earlier in the function as `src + (a value
read out of src's own header) + a constant`. This is a generic
"parse-a-table-header-then-normalize-its-arrays" routine, not something
that ever references `0x823F0C30` literally -- the corruption is entirely a
function of what `src` and its header bytes contain at the time it runs.

## Confirmed live: `src` is the exact buffer a zero-length `NtReadFile` never filled

A second, entry-level diagnostic on `sub_82234B88` printed `r3`, `r4`, and
`src[4..7]` at the moment it runs; a third, on `NtReadFile`, printed every
call's handle, length, and result. In the same run:

```
[r133] NtReadFile handle=0x00000002 offset=0 dest_guest=0x173a0020 length=0 known=1 bytes_read=0 status=0x00000000
[r133] sub_82234B88 entry dest(r3)=0x829ddd88 src(r4)=0x173a0020 hdr[4..7]=fe fe fe fe
```

`sub_82234B88`'s source pointer (`0x173a0020`) is **the identical guest
address** `NtReadFile`'s one call targeted. That call requested `length=0`
and (correctly, given the request) copied zero bytes -- `known=1` (the
handle from `NtCreateFile("DATA.TBL")` is valid), `status=STATUS_SUCCESS`,
`bytes_read=0`. **`sub_82234B88` then parses this never-written buffer as
if it held a real header.** The bytes it reads (`fe fe fe fe`) are a
classic uninitialized/poison pattern, not title content -- this project's
own `guest_memory_`/`guest_address_space_` never claims to zero-fill on
allocation, and nothing here suggests it should; the bug is upstream of
that.

This closes the causal chain completely: **zero-length read -> unfilled
buffer -> poison bytes read as a real stride field -> wild array-base
pointer -> in-place byte-swap loop scribbles across the critical-section
and notification-list memory two library functions away -> `sub_821F7C80`
(r131's crash site) walks the corrupted sentinel -> SIGSEGV.** Nothing
about this is a race or nondeterminism; given the same zero-length read,
this reproduces identically every time (confirmed 2/2 in r131, again this
cycle).

## Why does the game issue a zero-length read at all?

That is the one link in this chain not yet explained, and per this
project's discipline it is named rather than guessed at. A `NtReadFile`
call requesting 0 bytes is not itself invalid NT usage (some real
callers probe status this way), but `sub_82234B88` should not be reachable
with an unfilled buffer if the game's own logic is sound -- something
upstream is expected to have supplied a real, nonzero length derived from
the file's actual size (14,824 bytes for `DATA.TBL`, per r130's own
`read_directory` measurement) before this parse runs. `NtCreateFile` in
this project's implementation (`tools/materialize_native_import_stubs.py`)
returns only a handle and `STATUS_SUCCESS`/`STATUS_OBJECT_NAME_NOT_FOUND`
-- no size. Real Win32/NT titles typically learn a file's size via a
distinct query (`NtQueryInformationFile`, `GetFileSizeEx`, or an
IoStatusBlock field this project's `NtCreateFile` does not populate); none
of those are implemented here yet. The most likely mechanism is that the
game's size-query path returns 0 (or is entirely unimplemented and falls
through some default), and it then requests exactly that -- zero --
bytes, never treating this as an error because our own `NtReadFile`
answers "success" for it.

## Decision

No native code changed this cycle -- three rounds of temporary,
env-gated instrumentation, all reverted and verified reverted (`ctest`
9/9, 139/139 Python, clean `git status` on retail source). The concrete
finding is the full causal chain from r131's fix to r132's new crash to
this cycle's identified writer, closing what r132 explicitly left open.
Per this project's discipline against guessing a fix ahead of the
evidence, the actual size-query mechanism the game expects is named as
next step rather than implemented speculatively.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. Python suite: 139/139. `git status` on
`recompilation/ace-combat-6-retail`: only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change -- no source diff, since no
committed file was touched this cycle.

## Next

1. Find what the game calls between `NtCreateFile("DATA.TBL")` succeeding
   and the zero-length `NtReadFile` to determine the intended read size --
   disassemble the call site inside `sub_821CC508`'s retry chain that
   issues this read, tracing backward to whatever supplies its length
   argument (`ctx.r9.u32`). Do not assume `NtQueryInformationFile` without
   confirming it from the actual call sequence.
2. Implement whatever real size-reporting mechanism that call needs,
   sourced from the same `NativeGuestMediaService`/`locate_xdvdfs_file`
   this cycle's r130/r131 work already added (the real size, 14,824 bytes
   for `DATA.TBL`, is already known to the runtime at `NtCreateFile` time
   -- it is a question of exposing it through the right import, not of
   discovering it again).
3. Once the game issues a real-length read, re-run the probe against the
   qualified ISO and check whether `sub_82234B88` now parses genuine
   `DATA.TBL` bytes without producing a wild pointer, and whether the
   `sub_821F7C80` crash is gone -- do not assume it is closed until
   verified live, the same discipline r131 applied to r100's original
   crash.
