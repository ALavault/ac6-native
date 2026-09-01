# AC6 retail NTSC-U/J — `sub_82390880`'s `NtQueryInformationFile`/`NtSetInformationFile` usage is a debug movie-capture feature, confirmed unrelated to DATA.TBL; not implementing (r147)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle -- static disassembly
reading plus one `DumpBytes.java` read of the static XEX image.

## Following r146's own named next step

r146 named tracing `sub_82390880`'s callers before deciding whether
implementing real `NtQueryInformationFile`/`NtSetInformationFile`
support was worth building file-position-tracking infrastructure for.

`sub_82390880` has exactly two call sites, both inside a looped,
mode-dispatched file-open helper (`sub_821F4C10`, called from
`sub_821E9F50`/`sub_821EA2F8`) that builds a filename via `sprintf` with
format string `"%d%s"` (numbered file segments) before opening and
truncating each one.

## The decisive evidence

Reading the bytes immediately following that `"%d%s"` format string in
the static XEX image (`DumpBytes.java` at `0x82068170`) finds, at
`0x82068178`:

```
"D3D: Unable to create movie capture file segment %s.\n"
```

**This entire file-open/truncate loop is a debug/developer movie-capture
feature** -- numbered video segment files, written and truncated via the
same `sub_82390880` idiom this cycle traced, with a diagnostic log
message for the failure case sitting in the same string table
immediately adjacent to the filename format. This is unambiguous,
direct evidence from the retail binary's own data, not inference.

## Decision

`NtQueryInformationFile`/`NtSetInformationFile` are **not** implemented
this cycle, and this specific lead is closed. The call site r146 flagged
as newly-reached (`sub_82390880`) is confirmed to belong to a debug
movie-capture path structurally unrelated to `DATA.TBL`, the retry loop
(`sub_821D5F48`/`sub_821CC508`) this investigation has traced since
r117, or the `sub_821F7C80` crash chain (r130-r142). Building file-
position-tracking infrastructure in `NativeGuestMediaService` to service
a debug capture feature this project's own Gate 2 goal (reach visible
gameplay) has no use for would be exactly the kind of scope creep this
project's discipline forbids. This closes r146's open thread with a
confirmed negative, not an unresolved guess.

## Gates

No native code changed this cycle; `ctest`/pytest state is unchanged
from r145/r146's own last-verified clean run (`ctest` 9/9, 140/140
Python). `git status` shows only the pre-existing, unrelated
`upstream/AC6_recomp` submodule pointer change.

## Next

r144's determination -- no actionable, in-scope Gate 2 work currently
available beyond what r145's real fix already delivered -- is
reconfirmed for this specific thread. A future cycle should look for
other observable effects of r145's semaphore fix (per r146's own
methodology: check whether newly-reached imports connect to the active
crash chain before investing in them) rather than continuing to trace
this now-closed movie-capture lead.
