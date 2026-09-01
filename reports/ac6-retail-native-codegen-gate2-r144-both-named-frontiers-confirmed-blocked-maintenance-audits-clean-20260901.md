# AC6 retail NTSC-U/J — both named frontiers confirmed blocked; routine maintenance audits clean; no actionable Gate 2 work currently available (r144)

Date: 2026-09-01.

## Qualification

Ghidra project `ghidra-projects/ac6-us` (not touched this cycle), XEX US
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`. No
oracle used. No native code changed this cycle -- this is a scoping
check plus routine audit tool runs, not a diagnostic-instrumented probe
run.

## Checking r143's own pivot before acting on it

r143 pivoted the active frontier to `IM_LOAD_IMMEDIATE` Xenos->SPIR-V
translation, reading it as the next open `NEXT.md` line. Before starting
work on it, this cycle checked two things r143 did not verify: whether
the translator is actually implementable right now, and whether the
probe can even reach the code path it would serve.

**It is explicitly policy-blocked.** `native/include/ac6/native_shader_translator.h`
and its implementation already exist; `ShaderTranslator::translate` hard-
refuses `ShaderFormat::kXenosMicrocode` unconditionally, with its own
comment stating why: "Xenos microcode remains an explicit fail-closed
input until its AC6 fetch signatures are qualified against the pinned
offline golden oracle." This project has used no oracle for its entire
campaign (`CLAUDE.md`'s own qualification line on every report: "No
oracle used"), so this is a genuine qualified blocker under this
project's own definition -- work that cannot proceed without an external
resource (an oracle session) this campaign does not have.

**It is also currently unreachable.** `NEXT.md`'s own closing line for
this item already says as much ("Ne pas optimiser avant le début visible
de gameplay"), and the probe's actual behavior confirms it: the crash
this whole investigation traced through r130-r143 happens during early
resource loading (`DATA.TBL`/`sub_821F7C80`), well before the game would
ever reach GPU command submission. Implementing shader translation now
would have no probe to exercise it against.

## Checking whether r143's crash-chain closure was too hasty

Before accepting "no actionable work" as the conclusion, this cycle
re-examined whether there is a legitimate fix available for the
`sub_821F7C80` crash chain that r142/r143 did not consider: could
`sub_821CC288` (the guest function whose unchecked allocation failure
starts the whole cascade) be given a defensive check? No -- `sub_821CC288`
is retail guest code, not this project's own runtime; r142/r143's
explicit reasoning (do not patch a symptom in code this project does not
own) still holds, and there is no native-runtime lever available for a
crash whose root cause is guest-side stack content this project has no
established way to compare against real hardware.

## Routine maintenance audits, run as a sanity check

With both named frontiers confirmed blocked, this cycle ran the
project's own cheap, always-safe audit tools to check for any other
concrete, actionable gap:

```
audit_claude_md_numbers.py CLAUDE.md            -> pass, 3 checked, 0 mismatched
audit_contract_derivations.py analysis/contracts/*.json  -> pass, 52 behaviours, 0 gaps
audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json -> pass, 321 cited, 321 supported
audit_instrument_discipline_index.py INSTRUMENT_DISCIPLINE.md -> pass, 36 shapes, 0 unindexed
audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json -> FAIL, 3 drifted paths
```

The one failure is not new or actionable: all three drifted paths
(`reconstruction/ace-combat-6/include/ac6/ntxr_texture.h`,
`reconstruction/ace-combat-6/include/ac6/retail_flight_orientation.h`,
`reconstruction/ace-combat-6/src/retail_session.cpp`) are under the N2
`reconstruction/ace-combat-6` tree, which `NEXT.md`'s own "Frontières"
section states plainly is "abandoned for cette feuille de route; sa
preuve reste historique et aucune de ses sources n'est fusionnée." This
is the same pre-existing, unrelated condition every cycle since r107 has
already reported for `retail_session.cpp` specifically via the mission01
gate; this cycle confirms the other two paths are the identical kind of
already-known, out-of-scope drift, not new findings.

## Decision

No actionable, in-scope, non-blocked Gate 2 work is currently available:
shader translation is genuinely blocked by this project's own qualified-
blocker criteria (external oracle dependency) and by the probe's own
inability to reach that code path yet; the crash chain r130-r143 traced
has no further concrete lever without either an oracle or a change of
scope this project's own discipline forbids; and routine maintenance
audits surface nothing new. Per this project's own qualified-blocker
provision, this is named explicitly rather than papered over with a
lower-value busywork task.

## Gates

No native code changed this cycle; `ctest`/pytest state is unchanged
from r143's own last-verified clean run. `git status` shows only the
pre-existing, unrelated `upstream/AC6_recomp` submodule pointer change.

## Next

A future cycle should re-check this determination when either of two
things changes: (1) an oracle session becomes available for this
campaign (qualifying real Xenos fetch-signature translation), or (2) new
evidence emerges that the probe can progress past its current crash
point through means this cycle has not considered. Absent either, further
firings of the standing loop at the current cadence would only re-derive
this same conclusion; the loop is being stopped rather than continuing to
poll for no new information.
