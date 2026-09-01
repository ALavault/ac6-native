# AC6 retail NTSC-U/J — r145's semaphore fix reaches genuinely new ground: `NtQueryInformationFile`/`NtSetInformationFile` now reached for the first time, tied to a real "truncate to current position" idiom (r146)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle -- a read-only import
trace against the already-committed r145 binary, plus static reading of
the newly-reached call site.

## Following r145's own "next" item

r145 named checking whether its semaphore fix has any observable effect
downstream, even though it does not change the `sub_821F7C80` crash's
outcome. A full `AC6_NATIVE_IMPORT_TRACE=1` run against the qualified
ISO confirms it does: this run reaches `NtQueryInformationFile`,
`NtSetInformationFile`, `NetDll_XNetStartup`, `NetDll_WSAStartup`,
`XamShowMessageBoxUIEx`, `ExRegisterTitleTerminateNotification`, and
`NtSetInformationFile` -- **none of which appear in any of this
investigation's prior recorded traces** (r130-r133's own traces list a
different set: `XamLoaderLaunchTitle`, `XamShowDirtyDiscErrorUI`,
`VdGetSystemCommandBuffer`). This is direct evidence the semaphore fix
changed real guest scheduling/progress, not merely internal bookkeeping
-- some code path that previously stalled on an always-failing semaphore
wait now runs measurably further, even though the specific
`sub_821F7C80` crash this investigation has tracked since r131 is
unaffected.

## What the newly-reached call site actually does

`NtQueryInformationFile`'s one call site (`sub_82390880`,
`0x82390880`) confirms the real, standard NT signature directly from
this XEX's own disassembly: `r3=Handle, r4=&IoStatusBlock, r5=&FileInformation,
r6=Length, r7=FileInformationClass` -- exactly the documented API. The
function issues three calls in sequence, each gated on the previous
succeeding (`cmpwi r3,0; blt ... -> bail out`):

1. `NtQueryInformationFile(handle, ..., class=14)` -- `FilePositionInformation`,
   an 8-byte `LARGE_INTEGER` current-offset value.
2. `NtSetInformationFile(handle, ..., class=20)` -- `FileEndOfFileInformation`,
   set to the position just read.
3. `NtSetInformationFile(handle, ..., class=19)` -- `FileAllocationInformation`,
   also set to that same position.

This is a standard "truncate this file handle to its current position"
idiom -- read the current offset, then set both end-of-file and
allocation size to match it. It is not obviously connected to
`DATA.TBL`/the `sub_821F7C80` crash chain this investigation has traced
since r130; it more plausibly belongs to a save-file or log-file
open/reset path this project's existing `AtomicSaveStore` infrastructure
already has some machinery for.

## Decision

This is recorded as a genuine, confirmed finding -- not guessed at --
but not acted on this cycle. Implementing `NtQueryInformationFile`/
`NtSetInformationFile` for real would require adding guest-visible
file-position tracking to `NativeGuestMediaService` (this project's
existing `NtReadFile` always uses an explicit caller-supplied byte
offset, never an implicit stream position, so no such tracking exists
yet), and this specific call site's relevance to Gate 2's actual
blocking crash is not established. Implementing it now, without a
traced connection to a concrete blocker, would be scope creep beyond
what this cycle's evidence supports.

## Gates

No native code changed this cycle; `ctest`/pytest state is unchanged
from r145's own last-verified clean run (`ctest` 9/9, 140/140 Python).
`git status` shows only the pre-existing, unrelated `upstream/AC6_recomp`
submodule pointer change.

## Next

1. Determine whether `sub_82390880`'s truncate-to-position idiom is part
   of the save-game path this project's `AtomicSaveStore` already
   models, or an unrelated log/temp-file mechanism -- trace its callers
   before deciding whether implementing real
   `NtQueryInformationFile`/`NtSetInformationFile` support is worth the
   position-tracking infrastructure it would require.
2. If it is confirmed unrelated to Gate 2's active crash chain, this is
   lower priority than direct progress on the `sub_821F7C80` crash
   itself; do not implement it speculatively.
3. Continue treating "did this fix change observable guest behavior
   anywhere, even without fixing the specific crash under investigation"
   as a standing check after any real native-runtime fix -- this cycle's
   own finding came directly from following through on that check rather
   than treating r145's "insufficient to change the crash" as the end of
   the story.
