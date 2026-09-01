# AC6 retail NTSC-U/J — `read_xdvdfs_file`'s own `maximum_size` parameter check rejected every open regardless of file size; fixed by streaming instead of eager-copying; r100's original crash site (`sub_821D6C20`) is confirmed gone, replaced by a new deterministic crash in `sub_821F7C80` (r131)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. Native code changed for real this cycle:
`native/include/ac6/native_xdvdfs.h`, `native/src/native_xdvdfs.cpp`,
`native/include/ac6/native_guest_media.h`, `native/src/native_guest_media.cpp`.

## The bug, found by direct measurement, not guessed

r130 left "why does `read_xdvdfs_file` report 'not found' for
`DATA00.PAC`/`DATA01.PAC`/`DATA.TBL`" as the next question. Rather than
guess, this cycle built a throwaway standalone diagnostic
(`/fastdata/tmp/claude-1007/xdvdfs-diag/diag.cpp`, compiled directly against
`native_xdvdfs.cpp` as its own translation unit, not committed) that calls
the file's own internal `read_directory` against the real qualified ISO and
dumps every root entry it finds. **All 13 root entries are found correctly**,
`DATA00.PAC`/`DATA01.PAC`/`DATA.TBL` included, with real sector/size values
(`DATA00.PAC` sector=669663 size=2266267648, `DATA01.PAC` sector=2662740
size=664141824, `DATA.TBL` sector=1776239 size=14824) -- so r129/r130's read
of the tree traversal was never broken.

A second diagnostic (`diag2.cpp`) called the real public
`read_xdvdfs_file(..., maximum_size=512*1024*1024, ...)` -- the exact call
`NativeGuestMediaService::open_file` makes -- and it failed for **every**
path tried, including `default.xex` itself (which boots correctly every
cycle) and the 14,824-byte `DATA.TBL`, all with the identical error `"XDVDFS
maximum extraction size is invalid"`.

The actual bug: `read_xdvdfs_file`'s own parameter-validation line,

```cpp
if (maximum_size == 0u || maximum_size > 16u * 1024u * 1024u) {
  fail(error, "XDVDFS maximum extraction size is invalid");
  return false;
}
```

rejects the **caller's declared bound** whenever it exceeds 16MiB -- it is
not a check on the file's actual size, and it runs before any directory
lookup. `NativeRuntime::boot()`'s own `default.xex` read passes exactly
`16u * 1024u * 1024u` (the boundary value, not over it), so it has always
passed. `NativeGuestMediaService::open_file` (r130, new) passed
`512u * 1024u * 1024u` -- deliberately far above the file sizes it expected
to need -- which is `> 16MiB` and so failed this check unconditionally,
independent of which file was requested or how large it actually was. This
is why r130's live trace showed "not found" for a 14KB file sitting in a
directory this same code already parses correctly: the traversal was never
reached.

## Why raising the constant is not the fix

Even a raised constant would not be enough: this product's own `DATA00.PAC`
is 2,266,267,648 bytes (~2.1GiB) and `DATA01.PAC` is 664,141,824 bytes
(~633MiB). `read_xdvdfs_file`'s contract is an eager, single-shot copy into
a `std::vector<std::uint8_t>` -- pulling either of those fully into memory
on every guest `NtCreateFile` is the wrong shape for a title package this
size, independent of what any cap is set to.

## The fix: locate, don't eagerly copy, for the media service's own reads

Added `locate_xdvdfs_file(iso, internal_path, XdvdfsFile& file, error)` to
`native_xdvdfs.h`/`.cpp` -- the same descriptor/directory/path/cycle
validation as `read_xdvdfs_file`, refactored out of it into a shared
`locate()` helper, but returning only `{iso_offset, sector, size}` with no
byte copy and no size cap (there is nothing to cap; no payload is read).
`read_xdvdfs_file` itself is unchanged in every observable respect --
same signature, same `maximum_size` contract, same three existing asserts
in `native_xdvdfs_tests.cpp` still pass unmodified.

`NativeGuestMediaService::open_file` (ISO mode) now calls
`locate_xdvdfs_file` instead of `read_xdvdfs_file`, and stores
`{streamed=true, iso_offset, size}` rather than a byte vector.
`NativeGuestMediaService::read_file` opens a fresh `std::ifstream` on the
bound ISO path per call for a streamed handle, seeks to
`iso_offset + offset`, and reads only the `length` the guest actually
requested for that one `NtReadFile` call -- matching real hardware's
streaming-read shape rather than a whole-file preload. Assets-directory
mode (dev-only small fixtures) is unchanged: still a full, bounded
(512MiB-capped) preload via `std::filesystem`/`std::ifstream`.

## Verified live against the real qualified ISO

```
[NtCreateFile] "DATA00.PAC" -> ok
[NtCreateFile] "DATA01.PAC" -> ok
[NtCreateFile] "DATA.TBL" -> ok
```

All three open successfully now (previously all three reported "not
found"). The probe continues well past every point any prior cycle in this
chain (r105-r130) reached, including background-thread imports
(`ExCreateThread` for handles 297/298/300), `VdGetSystemCommandBuffer`,
`VdSetDisplayMode`, and repeated re-opens of `DATA.TBL`/`DATA00.PAC`/
`DATA01.PAC` (the guest re-`NtCreateFile`s them multiple times, each
succeeding).

## r100's original crash site is confirmed gone

Every cycle since r116 hit a deterministic crash at `sub_821D6C20`
(dereferencing a null global `0x82935d98`). This cycle's probe runs past
that point entirely -- `sub_821D6C20` does not appear in the new crash's
backtrace, and the process makes substantially more forward progress
(more imports fire, more threads spawn) before failing elsewhere. This is
the first cycle in the whole r100-r131 arc where that specific crash did
not reproduce.

## A new, different, deterministic crash: `sub_821F7C80`

The probe now segfaults on one of the `ExCreateThread`-spawned background
threads (routine `0x821eede0`), reproduced identically across two
independent `gdb` runs:

```
Thread N "ac6recomp" received signal SIGSEGV, Segmentation fault.
0x00005555557a0a5e in __imp__sub_821F7C80 ()
#0  __imp__sub_821F7C80 ()
#1  __imp__sub_82390B18 ()
#2  __imp__sub_821F8008 ()
#3  std::thread::_State_impl<...ExCreateThread(...)::$_0...>::_M_run() ()
```

`rbp=0x0`, `rdx=0x0`, `r13=0x0`, `r15=0x0` at the fault -- shaped like a
null-guest-pointer dereference, not yet localized to a specific
instruction or field. This is plausibly related to processing the content
just successfully read (`DATA.TBL`/`DATA00.PAC`/`DATA01.PAC` -- table/
package parsing code is a reasonable guess for what `sub_821F7C80` is) but
that connection is not established from disassembly this cycle -- it is
named as the concrete next step, not asserted.

## New/updated tests

No new C++ or Python tests were added this cycle: `locate_xdvdfs_file`
reuses the same `locate()` internals `read_xdvdfs_file` already exercises
via `native_xdvdfs_tests.cpp`'s three existing asserts (which still pass,
unmodified), and `NativeGuestMediaService`'s streamed-read path has no
existing C++ test target of its own (the whole class was untested at the
unit level before this cycle too -- it is exercised only via the Python
generated-stub tests and this cycle's live probe). Adding a dedicated
`NativeGuestMediaService` streamed-read unit test is worth doing but was
not required to make or verify this fix, which is now proven by the live
probe's own qualified-ISO run; noted as follow-up, not done speculatively.

## Gates

`python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF`:
fails on the same pre-existing, unrelated `reconstruction/ace-combat-6/src/retail_session.cpp`
evidence-size mismatch every cycle since r107 has reported. `ctest`
(native profile): 9/9. `git status` (run after `ctest`, not before): no
regenerated metrics artefact came back modified-but-unstaged.
`git status --porcelain=v1 -- recompilation/ace-combat-6-demo | wc -l`: 185,
unchanged. Python suite: 139/139, unaffected (no Python source touched this
cycle).

## Next

1. Disassemble `sub_821F7C80` (verify completeness against `.pdata` first,
   per `CLAUDE.md`'s own discipline) to find the exact null dereference --
   which register/field is null and why, and whether it is genuinely
   downstream of the newly-loaded `DATA.TBL`/`DATA00.PAC`/`DATA01.PAC`
   content (e.g. an unparsed table format, a pointer this code expects some
   earlier step to have set) or an unrelated, previously-unreached surface.
2. Once the mechanism is understood, decide the narrowest real fix -- per
   this project's own discipline, no synthetic/hardcoded "solving" value.
3. `sub_82390B18` and `sub_821F8008` (the two callers on the same stack)
   are also unexamined; understanding what they were trying to do with
   `sub_821F7C80`'s result may be necessary context before the fix, not
   just the fault site itself.
