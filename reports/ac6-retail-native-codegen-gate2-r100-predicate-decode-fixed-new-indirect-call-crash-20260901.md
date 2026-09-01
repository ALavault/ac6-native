# AC6 retail NTSC-U/J — predicate rejection removed on verified evidence; probe now reaches a new indirect-call crash deep in `_xstart` (r100)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. Native code changed: `native/src/native_xenos.cpp`,
`native/tests/native_xenos_tests.cpp`.

## Continuing r99's frontier

r99 traced both predicated packets in the captured buffer to their
construction and found `WAIT_REG_MEM`'s predicate path entangled with the
already-documented (r85-r89) interrupt-callback gap. It named a bounded
static follow-up (trace `Function_821E6280`'s callers) and declined to
implement a fix without a verified source.

## Static trace attempted, then reframed

Started tracing `Function_821E6280`'s three callers toward the ultimate
origin of `r5` bit 2 (`FindDirectCallsTo.java`, then a manual instruction
dump of `Function_821E4AD0`, the common convergence point). The trace
reached a register (`r27`) assigned from four different sites
(`0x821e4b28`, `0x821e4c64`, `0x821e4ca4`, `0x821e4cac`) with the reaching
definition still undetermined after five levels of call-graph depth across
four cycles (r97-r100).

**An external review reframed the question before this went a level
deeper.** The captured buffer already contains the predicated
`WAIT_REG_MEM` packet — the runtime observation that bit 2 was set on some
real call already answers "does this content ever emit the predicated
form" (yes). Continuing the static trace could only re-derive that same
fact from a different angle; it could not answer the real open question,
which is what the *decoder* should do with a predicated packet, given the
codebase implements no predicate-condition-register infrastructure at all.

## The decision this cycle actually made, and its evidence

Removed the `native_xenos.cpp:169-171` rejection. Predicated TYPE3 packets
now decode identically to their unpredicated form — `opcode_of()` and
`count_of()` already mask off bit 0, so no other code needed to change.
This is stated as a **decode policy for the content this project has
actually observed**, not a hardware-predication claim, on four
independently-verified points:

1. **Both predicated packets' bits are fixed compile-time constants at
   their construction site**, never a computed condition (r99:
   `DRAW_INDX_2` at `0x821e14a0`; `WAIT_REG_MEM` at `0x821e637c`, guarded
   by a guest-side branch that either emits the packet with bit 0 set or
   skips emitting it entirely). There is no data-dependent predicate
   *value* lost by ignoring the bit.
2. **The full opcode census of this content (r96, 281 TYPE3 packets: 10
   distinct opcodes) contains no predicate-condition-setting opcode**, and
   this file's own `kOpcode*` table (`native_xenos.h`) defines none
   either. No captured instruction ever moves a predicate condition away
   from its reset state.
3. **The Vulkan backend's `WaitPacket` handler was already
   predicate-agnostic** (`native_vulkan_backend.cpp:104-109`): it checks
   only `packet.selector <= 7u`, with no reference to the header's
   predicate bit. Nothing downstream needed a decoder-side model of
   predication to keep working.
4. **The predicate-false alternative for `WAIT_REG_MEM` (r99) is a
   guest-side early return**, not a second runtime state this decoder must
   track: the branch that skips packet emission returns straight into the
   already-fully-characterized `sub_821E63F0` interrupt handler
   (r85-r89).

This is a narrower guarantee than true hardware predication — a future
capture with an actual predicate-setting opcode, or a computed predicate
value, would decode identically to its unconditional form. The code
comment states this gap explicitly rather than hiding it.

## Test changes

- Split the old `decoder_enforces_hardware_predicate_and_one_register`
  (two unrelated assertions under one name) into
  `decoder_enforces_hardware_one_register` (kept unchanged) and two new
  functions: `decoder_decodes_predicated_type3_like_unpredicated` (a
  predicated/unpredicated pair of the same packet must decode to the same
  output) and `decoder_accepts_real_captured_predicated_headers`
  (reproduces both real captured headers, `0xC0003601` and `0xC0043C01`,
  end to end).
- **Found and fixed a second, unrelated, pre-existing test bug** while
  verifying `ctest`: `indirect_buffer_decode_error_reports_real_hex_address`
  (introduced r95, edited r96) set the `INDIRECT_BUFFER` packet's dword
  count field (`ring[2]`) to `1u` while its guest content was 4 dwords —
  the decoder truncated the IB before ever reaching the register-range
  check the test exists to exercise, silently asserting
  `kTruncatedPacket` had the *code* of `kInvalidRegister`... no, silently
  failing with `kTruncatedPacket` instead of the intended
  `kInvalidRegister`. **Verified this predates r100 entirely**: reverted
  both this cycle's files via `git stash` and reran — same failure, same
  line, same wrong code, on r99's unmodified tree. Fixed by setting
  `ring[2] = 4u`. This test failure would have been discovered by any
  cycle that ran `ctest` on this file since r96; it happened to surface
  here because this cycle's edits were the first to rebuild and rerun this
  specific test binary since a `PPCFuncMappings` change or that gap.

## Verified

- `ctest` (native profile): **9/9** passed (was 8/9 before the
  `ring[2]` fix, for the pre-existing reason above, unrelated to the
  predicate change).
- `pytest` (`tests/`, scoped — the unscoped root collects the
  `upstream/AC6_recomp` submodule's vendored googletest Python suite and
  fails to import it; this is a pre-existing environment fact, not a
  regression): **130/130** passed.
- `audit_test_assert_liveness.py`: pass.
- `python3 tools/validate.py --target ntsc-uj --runtime native`: pass.
- `audit_ac6_mission01_native_gate.py` / `audit_ac6_contract_artifacts.py`:
  fail on the same pre-existing, unrelated N2 (`reconstruction/ace-combat-6`)
  evidence drift as r90-r99 — not this track, not touched.
- `audit_ac6_contract_addresses.py`: pass, `cited=321 supported=321
  unsupported=0`.
- `git status` after `ctest`: only the two intentionally-edited files
  under `recompilation/ace-combat-6-retail/native/` changed.

**Live probe result — real, substantial progress, with a new crash, not a
clean stop:**

```
vd publish write=49 read=43     (previous record, r96: 37)
```

Ring publish advanced through the predicate boundary and kept going: new
allocations at `0x16540000`-`0x16d70000` (six allocations up to 2MB, the
largest content this track has ever staged — texture/vertex-buffer scale,
not bootstrap scale), a `vd swap commit` event, and four further publish
cycles each incrementing a fence value (`7, 9, 11, 13`). This is
qualitatively different activity from every prior cycle back to r51.

The process then **segfaulted** (`exit=139`, not the usual bounded-probe
timeout). Core-dump backtrace (`gdb` on the `apport` core, not a live
attach — direct execution crashes inside the probe's real-time window in
a way a `gdb`-attached run did not reproduce at 60s or 180s, plausibly a
scheduling-sensitive path; the core dump is the reliable capture method
here):

```
#0  __imp__sub_821D6C20 ()
#1  __imp__sub_821D7DE0 ()
#2  __imp___xstart ()
#3  main ()
```

The fault is an indirect call through the generated function-pointer
lookup table (`call *(%rcx,%rax,1)`, the standard XenonRecomp `bctrl`
pattern) with `rax = 0x7bf62a8a0000` — a host-pointer-range value, not a
plausible 32-bit guest address. Whatever PPC register this came from was
never a valid guest code pointer at this call site.

## Decision

Implemented the predicate-decode fix (narrow, evidence-bounded, per the
four points above), not the broader "always execute" hardware claim r97
correctly declined without a verified source — the distinction matters:
this cycle's fix is a decode-time no-op on a bit nothing in the observed
content computes, not a claim about GPU predicate semantics in general.

Did not chase the new `sub_821D6C20` crash this cycle. It is deep inside
`_xstart`'s own guest boot path (not the Vd/PM4 pipeline this six-cycle
thread has been investigating), reached for the first time only because
this fix let decode proceed roughly 30% further into the ring content
than any prior cycle. Characterizing it needs its own static trace from a
clean state, which this cycle's budget does not have room for after the
predicate work and the second test-bug fix.

## Gates

All six required gates run, in the order CLAUDE.md specifies (`ctest`
before `git status`, contract/artifact audits after). Results above.

## Next

1. **New, concrete frontier**: `sub_821D6C20` (called from
   `sub_821D7DE0`, called from `_xstart`) performs an indirect call
   through a register holding a host-pointer-range value where a guest
   function pointer is expected. Static trace of `sub_821D6C20` from a
   clean Ghidra pass, and of what sets the register at the call site, is
   the next bounded question — not a continuation of the predicate
   thread, which is now closed for the content this project has
   observed.
2. r94's older open thread (identifying the object behind the
   `sub_821E6AC8`/`sub_821F03B0` wait chain) remains untouched and is now
   probably moot: the probe no longer reaches that chain before hitting
   the new crash, since decode now proceeds much further before the
   guest's own boot code (not the Vd pipeline) faults.
3. The `r27`/`Function_821E4AD0` predicate-origin trace started this
   cycle is abandoned, not paused — r99's predicate question is answered
   (decode policy implemented on independent, sufficient evidence), and
   the trace was never going to answer a different question than the one
   already settled.
