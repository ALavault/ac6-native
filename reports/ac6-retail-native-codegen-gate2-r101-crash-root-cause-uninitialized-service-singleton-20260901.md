# AC6 retail NTSC-U/J — the `sub_821D6C20` crash is a lazy-singleton service object read before construction, on this call path (r101)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle — static trace plus one
core-dump memory read, investigation only, for the reason given below.

## Continuing r100's frontier

r100 removed the predicate rejection and, in verifying it live, reached
substantially further into the guest's boot sequence than any prior cycle
before crashing on an indirect call through a wild pointer inside
`sub_821D6C20`, reached via `_xstart → sub_821D7DE0 → sub_821D6C20`. It
named this crash as the next frontier without characterizing it.

## The crash mechanism, fully traced

`DumpRange.java` against `ghidra-projects/ac6-us` (never the PAL demo
project) disassembled `sub_821D6C20` from its prologue. The faulting
`bctrl` is the first of over twenty near-identical blocks in this
function, all of the same shape:

```
821d6c2c  lis r30,-0x7d6d          ; r30 = 0x82930000
821d6c30  lwz r3,0x5d98(r30)       ; r3 = *0x82935d98  -- a global object pointer
821d6c34  lwz r11,0x0(r3)          ; r11 = r3's vtable pointer
821d6c38  lwz r11,0xe4(r11)        ; r11 = vtable slot 0xe4
821d6c3c  mtspr CTR,r11
821d6c40  bctrl                    ; virtual call -- this is the fault site
```

`count_indirect_branches.py` had earlier flagged two `bctrl`-looking
addresses in a naive byte range scan; the actual disassembly shows this is
one function with the pattern repeated roughly two dozen times (verified
via `FindInstructionScalar.java 0x5d98`: **26 hits total in the whole
executable, every one of them a load, every one of them inside this one
function** — this function is a large dispatcher calling many methods
through the same service object, not a small leaf).

The crash's own register dump (r100) had `rbp = 0x82935d98` exactly —
XenonRecomp's generated host code holds this constant guest address
directly in a host register across the unrolled access sequence, which is
what let this cycle identify the exact global from the crash alone,
without first finding the source.

## The global is a genuine null pointer at runtime, not a formatting artifact

`FindDataPointersTo.java 0x82935d98` (a full-executable scan for a
big-endian pointer aimed at this address): `hits=0` — nothing in static
data references this global by address, so it is a plain global slot, not
a known struct field. Its static XEX image content
(`DumpDataWords.java`): all zero, as expected for a lazily-constructed
singleton slot.

The important check is the **runtime** value, not the static one. Read
directly from the crash's own `apport` core dump (the same core r100
analyzed), dereferencing guest address `0x82935d98` through the crash's
own captured `base` (`$rsi = 0x7bf5a7e00000`):

```
(gdb) x/1xw ($rsi+0x82935d98)
0x7bf62a735d98: 0x00000000
```

**Genuinely zero at the moment of the crash**, not merely in the initial
image. The vtable dereference at `0x821d6c34` reads guest address `0`
(the pointer itself is null), and the wild `bctrl` two instructions later
is a direct, mechanical consequence: whatever ends up in `CTR` from
dereferencing a null object's "vtable" is not a valid guest code address,
and XenonRecomp's generated indirect-call lookup (`call *(%rcx,%rax,1)`)
has no bounds check against that.

## The real construction site exists, and is upstream but unreached on this path

`FindPpcAddressMaterialization.java 0x82935d98` found the one place this
address is built as a value rather than dereferenced with a displacement:
`0x821d6be0`/`0x821d6be4` (`lis r11,-0x7d6d; addi r11,r11,0x5d98`).
Dumping the surrounding block (`0x821D6B60`-`0x821D6C1C`) shows a
textbook lazy-singleton pattern immediately before `sub_821D6C20`'s own
prologue (a separate function, ending in a tail-jump to `0x82382a48`, not
a continuation of `sub_821D6C20`):

```
821d6bc8  bl 0x82222d80            ; likely allocator/factory
821d6bcc  cmplwi cr6,r3,0x0
821d6bd0  beq cr6,0x821d6bdc       ; already-constructed path
821d6bd4  bl 0x820b1cd0            ; construct
821d6bd8  b 0x821d6be0
821d6bdc  or r3,r23,r23            ; reuse cached r23
821d6be0  lis r11,-0x7d6d
821d6be4  addi r11,r11,0x5d98
821d6be8  stw r3,0x0(r11)          ; publish the singleton -- the ONE store site
821d6bec  lwz r11,0x0(r3)
821d6bf0  lwz r11,0xe0(r11)        ; a DIFFERENT vtable slot (0xe0) succeeds right here
821d6bf8  bctrl
```

This confirms the object is real, constructible, and even used successfully
(slot `0xe0`) immediately after construction, in whatever function this
block belongs to. The question is whether that function runs before
`sub_821D6C20` on the path this probe actually takes.

## `sub_821D7DE0` never reaches it

`FindDirectCallsTo.java 0x821D6C20`: **exactly one caller**,
`0x821d7e40` inside `sub_821D7DE0` — matches r100's crash backtrace.
Dumped `sub_821D7DE0` in full (`0x821D7DE0`-`0x821D7E44`): it calls
`sub_821D5F48` (a flag check), conditionally `sub_821F5B18` (gated on that
flag, likely a debug/logging path), then a 2-iteration loop of
`sub_82331DE8`/`sub_82331D90`/`sub_82331E30`, then directly
`sub_821D6C20`. **None of these is the singleton-construction block above,
and none of their addresses fall inside it.** The one guest-code path this
probe exercises from `_xstart` reaches the read site without ever passing
through the one confirmed write site.

## Decision

This is stated as a **precisely bounded gap**, not a guessed fix, for the
same reason r97 declined to guess predicate semantics: three genuinely
different explanations fit the evidence gathered so far, and nothing here
distinguishes them:

1. Something earlier in `_xstart`'s broader boot sequence (before
   `sub_821D7DE0` is ever called) is supposed to run the construction
   block, and this probe's entry path — a single deterministic guest
   thread via `initialize_probe_thread`, not the retail title's real
   multi-threaded boot — never reaches it.
2. The real console boots this object on a **different guest thread**
   concurrently, and by the time `sub_821D6C20` runs, a race normally
   resolves in the singleton's favor — the native harness's single-thread
   probe model has no second thread to win that race. `RESUME.md`/prior
   cycles already document this project's harness as deliberately
   single-threaded for the entry probe.
3. The construction depends on an import/kernel/XAM stub this harness
   still no-ops, whose real implementation is what would trigger the
   construction call in the first place.

Distinguishing these needs either a second, real guest thread in the
probe harness, or tracing every caller of the construction block's own
containing function (not yet identified by name/address — only located by
the block's end at `0x821d6c1c`) to see what's actually supposed to invoke
it and when. Both are substantive, scoped follow-ups, not this cycle's
remaining budget.

## Gates

No native code changed this cycle. `ctest`/pytest not re-run (nothing to
re-verify); `git status` confirms no files touched.

## Next

1. Identify the containing function of the `0x821d6b60`-`0x821d6c1c`
   singleton-construction block (search backward for its prologue, or via
   `.pdata`/xrefs), then trace **its** callers — this answers whether the
   construction is reachable at all on any static path from `_xstart`, or
   only from a thread/subsystem this harness doesn't spin up.
2. If reachable only via a second guest thread, that becomes a concrete,
   scoped harness change (spawn that thread, not a synthetic value) —
   worth doing only after (1) confirms it's the actual gap, per this
   project's standing rule against fixes built on an inferred mechanism.
3. r100's other named thread (the old `sub_821E6AC8`/`sub_821F03B0` wait
   chain, r94) stays probably moot — this probe no longer reaches it
   before the new crash.
