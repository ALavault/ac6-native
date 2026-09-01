# AC6 retail NTSC-U/J — cost/benefit check closes the DATA.TBL sub-thread at its current depth; pivoting to the next Gate 2 frontier (r143)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle -- this is a read-only
static check (`grep`, direct file reads) plus a decision, not a
diagnostic-instrumented probe run.

## The check r142 named

r142 left the DATA.TBL sub-thread with `sub_82338388` returning a
sign-extended read of stack memory (`[r1+88]`) that nothing in the traced
call chain writes, and named checking whether *other* callers of the
same wait primitives (`sub_821F4128`/`sub_821F7538`,
`NtWaitForSingleObjectEx`) show a matching "read a paired stack slot
right after the wait" pattern -- which would suggest a real
`IO_STATUS_BLOCK`-style output convention this project's HLE stub fails
to populate, versus `sub_82338388` being an isolated case.

`sub_821F4128` has callers in **twelve** different generated files --
far more than a narrow, DATA.TBL-specific helper; it is a generic
"wait, ignore the result" wrapper used throughout the retail binary.
Reading one representative call site (`ppc_recomp.11.cpp:11858`) shows
the pattern that is actually typical: the code proceeds immediately to
unrelated work (loading a completely different global) with no read of
any stack slot adjacent to the wait call. **This is evidence against the
`IO_STATUS_BLOCK` convention theory**, not for it -- if this were a
documented, load-bearing output-parameter convention, other callers
using the same generic wait wrapper would plausibly also consume it, and
this one sampled site does not.

## The decision

This sub-thread (r130-r142, thirteen cycles) has fully traced a real
crash from `sub_821F7C80`'s SIGSEGV back through eleven distinct, each
individually-measured mechanisms to a single leaf: `sub_82338388`
returning uninitialized stack content because a kernel wait it issues
times out and nothing populates the value it reads afterward. Every link
in that chain is backed by a live measurement, and several of this
investigation's own intermediate conclusions were caught and corrected
within the same or a following cycle: r135 caught its own mid-cycle
mislabeling of a function boundary, r138 and r140 each tested and
refuted a specific hypothesis live rather than accepting a plausible
static read, and r141 corrected a load-bearing premise r139 had
asserted. This is a complete, well-evidenced case study by this
project's own standards.

Continuing further into this one leaf -- e.g., exhaustively checking all
twelve `sub_821F4128` call sites, or trying to determine whether real
Xbox 360 hardware's stack-reuse pattern would leave different content at
this exact byte offset -- has sharply diminishing returns: even a
conclusive answer would only explain *why this recompilation's stack
layout differs from retail hardware's*, a question this project has no
established technique to answer without an oracle (explicitly out of
scope for this whole campaign), and would not itself point to a
concrete, actionable fix distinct from what r130-r131 already delivered
(the real, shippable improvement in this whole arc: `NtCreateFile`/
`NtReadFile` against real media, and the XDVDFS `maximum_size` fix).

Per this cycle's own judgment, continuing this specific sub-thread past
r142 is not the best use of further cycles. This cycle closes it at its
current, fully-documented depth and pivots the active frontier back to
the broader Gate 2 backlog `NEXT.md` already carries.

## What remains true and committed from this whole sub-thread

r130-r131's fixes (`NativeGuestMediaService`, `NtCreateFile`/`NtReadFile`
against real media, the XDVDFS `maximum_size` validation fix) are real,
tested, and already closed r100's original crash -- that result stands
independent of this sub-thread's later, deeper tracing. The new crash
`sub_821F7C80`/r131 onward exposed is real and reproducible, but it is
now understood to terminate in retail-code stack-content behavior this
project cannot resolve further without disproportionate effort; it is
left open, fully documented, rather than patched with an unverified
guess (which this project's discipline forbids regardless).

## Gates

No native code changed this cycle; the mandatory build/test gates were
not re-run since nothing in the tree changed since r142's own clean,
verified state (`ctest` 9/9, 139/139 Python, confirmed at r142's close).
`git status` unchanged from r142.

## Next

The active Gate 2 frontier moves to `NEXT.md`'s next-oldest open item:
`IM_LOAD_IMMEDIATE` Xenos->SPIR-V translation, the item every recent
`NEXT.md` entry has continued to list as the closing, still-open line of
the backlog. The next cycle should read that item's existing context
(prior reports referenced from `NEXT.md`/`RESUME.md` around it, if any)
before starting fresh investigation, following this project's own
qualification discipline.
