# AC6 retail NTSC-U/J — event-family callers settled as ordinary engine code; DbgPrint visibility added (r92)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us`, XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. Native code changed: `tools/materialize_native_import_stubs.py`
(new `DbgPrint` handler), `tests/test_materialize_native_import_stubs.py`
(one new test).

## Closing r91's open question

r91 left open whether the measured `NtSetEvent`/`NtClearEvent`/`wait_event`
traffic (~5,000 calls/s) was legitimate engine signaling or a guest-side
spin. This cycle traced it statically rather than guessing further.

Guest thunk addresses (from `ppc_func_mapping.cpp`): `NtSetEvent` =
`0x823D015C`, `NtClearEvent` = `0x823D017C`. `FindDirectCallsTo.java` against
`ac6-us` found:

- `NtSetEvent`: **zero** direct static callers anywhere in the image.
- `NtClearEvent`: **one** direct caller, `sub_821F4210` (`0x821f421c`).

`sub_821F4210` itself has **nine** distinct static callers, across six
different parent functions of varying, ordinary size (80-368 instructions):
`Function_82204388` (368), `Function_821FEA78` (224), `Function_821D4988`
(160), `Function_821D4A28` (80), `Function_821D4B30` (240),
`Function_821D4C20` (180, one of r90's newly-active worker threads),
`Function_821D4F20` (308, called twice, also from r90), and
`Function_823CEE90` (136). This is a diverse, ordinary per-subsystem
"reset a status flag" call pattern, not one naked function looping on
`NtClearEvent` in a tight cycle.

`NtSetEvent` having zero static callers means it is reached only
indirectly — consistent with r91's separate finding that `sub_821F7C80`
walks a linked list of registered callbacks and invokes each via `bctrl`.
This is the same shape already characterized in r91 as an ordinary
notification-dispatch mechanism, not re-examined further this cycle.

**Conclusion**: the measured event-family traffic comes from a spread of
distinct, ordinary-looking engine call sites (a "clear my status flag"
helper called from six unrelated subsystems, plus generic callback
dispatch for signaling), not from a single degenerate spin loop. This
settles r91's open question as a **negative**: no guest-side spin bug was
found in the static call graph. The traffic's volume is best explained by
the probe running with no frame-rate governor (nothing paces these worker
threads to a target Hz), which is expected for an offline, unthrottled
bounded probe and not itself a defect to fix.

## DbgPrint: added visibility, verified safe, not yet exercised

`DbgPrint` had no specific handler (silent, generic `kOfflineStatus`
fallback). Its two static call sites were disassembled to determine the
calling convention before implementing anything:

- `sub_821EF458` (`0x821ef4a4`): a `DbgPrintf`-style convenience wrapper —
  spills `r4`-`r10` to a stack `va_list`-shaped area, calls a
  `vsnprintf`-like helper (`0x823d044c`) to format into a stack buffer
  **before** calling `DbgPrint` with the already-resolved string. No
  varargs remain live at the actual `DbgPrint` call.
- `sub_821F5ED0` (`0x821f607c`): calls `DbgPrint` with a literal static
  string address in `r3` and one raw integer in `r4` — consistent with
  `DbgPrint` itself supporting `printf`-style formatting (matching the
  documented Xbox 360 kernel `DbgPrint(const char*, ...)` signature).

Implementing full PPC-ABI varargs substitution inside the stub was judged
not safely verifiable from two call sites alone (risk of misreading guest
registers as the wrong argument type, or overrunning on a malformed
format string) — this would be exactly the kind of unverified mechanism
the project's evidence discipline refuses. Instead, `DbgPrint` now dumps
the **raw** guest format string (bounded 512-byte read via `PPC_LOAD_U8`,
no `ctx.r4`+ substitution), gated by the existing `AC6_NATIVE_IMPORT_TRACE=1`
diagnostic. This is safe in both cases: call site 1's message is already
fully resolved before `DbgPrint` sees it; call site 2's raw string is still
informative even with an unsubstituted `%d`-style placeholder.

**Verified via the existing bounded probe (25s, `AC6_NATIVE_IMPORT_TRACE=1`):
zero `[DbgPrint]` lines were printed.** Neither static call site is reached
in this window. This is reported as a plain negative, not a visibility gain
this cycle — the addition is kept because it is safe, tested, and useful
for a longer probe or a different guest code path in a future cycle, not
because it produced new information this time.

## Gates

- `audit_ac6_mission01_native_gate.py`: fails on the same pre-existing,
  unrelated N2 evidence mismatch as r90/r91
  (`reconstruction/ace-combat-6/src/retail_session.cpp`, not touched).
- `ctest` (native profile): **9/9** passed.
- `pytest` (full retail suite): **130/130** passed (129 prior + 1 new:
  `test_dbg_print_dumps_raw_guest_string_without_synthesizing_varargs`).
- `git status` after `ctest`: only the two intentionally-edited files
  changed; the pre-existing, unrelated `upstream/AC6_recomp` submodule
  pointer change was left untouched.

No scripts left behind (`DumpR92Callers.java`, `DumpR92DbgPrint.java` were
used read-only against `ghidra-projects/ac6-us` and deleted; confirmed by
`git status --porcelain=v1 -- scripts/` showing empty).

## Next

With r91's open question settled (negative) and DbgPrint visibility added
but unexercised, there is no further narrow lead on the event-traffic
thread to chase. The next cycle should return to surveying the remaining
generic-fallback imports by measured frequency (`AC6_NATIVE_IMPORT_TRACE`,
same method as r90) over a longer bounded window, or consider whether a
minimal frame-pacing governor is worth adding before continuing to chase
individual import families — a scope decision for the next cycle, not
made here.
