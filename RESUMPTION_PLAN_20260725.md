# AC6 — resumption plan: unblock the runtime before resuming codegen

> **SUPERSEDED — 2026-08-08, cycle 1143.** Entirely overtaken. Its Phase 1 question — characterise the `bad_alloc` — was answered at cycle 303 (not reproducible); its Phase 3 deliverables `functions.csv` and `subsystems.csv` were never produced and are not wanted; and its opening premise, that the target is frozen during an AC5 focus, no longer holds.
>
> The live roadmap is **`MISSION01_LADDER.md`**. The gates that actually run are
> `analysis/contracts/mission01-final-gate-v3.json` (JF) and
> `analysis/contracts/mission01-native-gate-v2.json` (J0/J1), audited by
> `tools/audit_ac6_mission01_native_gate.py`. Working rules are in `CLAUDE.md`.
>
> This file is kept for its history. Do not plan from it.

Written 2026-07-25. A re-prioritisation, not a replacement: the durable phases,
workstreams and gates stay in `DECOMPILATION_PLAN.md`. Proof rules stay in
`AGENTS.md` and `workspaces/ace-combat-6/AGENTS.md`.

Target is frozen during the AC5 focus. This plan applies on resumption.

## 1. Measured state, 2026-07-25

| Measure | Value | Source |
| --- | ---: | --- |
| Current cycle | 302 (`runtime_blocked`) | `reports/handoff/CURRENT.json` |
| Cycle reports | 242 | `workspaces/ace-combat-6/reports/` |
| Native code | 7 330 LOC | `reconstruction/ace-combat-6/{src,include}` |
| Test files / CTest | 29 files, 48/48 | best ratio of the four targets |
| **Generated corpus** | **23 321 implementations, 50 TUs** | `DECOMPILATION_PLAN.md` baseline |
| **`functions.csv` register** | **absent** | verified |

Build hygiene: `-Wall -Wextra -Wpedantic -Wconversion`, zero warnings, but
**no `-Werror`** and no sanitizer option. Pharaoh and Ski both enforce
`-Werror`; Ski also wires ASan/UBSan.

## 2. Diagnosis

**AC6 is the one target whose spine actually exists.** The ReXGlue runtime
links on Linux and reaches recompiled PPC code, and the generated corpus holds
23 321 implementations across 50 translation units. This is the architecture
AC5 has been told to build and has not: for AC6 it is already standing. AC6 is
closest to a real native executable of the four.

**It is blocked inside a third-party SDK, not obviously inside AC6.** Cycle 302
reports that ReXGlue announced 23 320 functions and then terminated with
`std::bad_alloc`. The current `next_action` preserves the cycle-301 rollback
and forbids rerunning codegen — correct as a freeze, but it leaves the failure
attributed by proximity rather than by diagnosis. A `std::bad_alloc` raised
after enumerating 23 320 functions is at least as consistent with host memory
exhaustion during SDK processing at scale as with a defect introduced by the
`0x82345250` boundary removal.

**The SDK is consumed second-hand and unpinned.** ReXGlue reaches AC6 through
`.tools/ac6-recomp-reference/thirdparty/rexglue-sdk`, vendored inside the
third-party fan project [sal063/AC6_recomp](https://github.com/sal063/AC6_recomp),
at that project's state of 2026-07-16. AC6 does **not** track upstream
[rexglue/rexglue-sdk](https://github.com/rexglue/rexglue-sdk) directly, and
upstream has an active issue tracker and API documentation. A fix or a known
limitation upstream would be invisible here.

ReXGlue maps the guest's 4 GB address space into host memory via a
memory-mapped file at a fixed host address, with guest allocation through
`REX_PPC_HEAP_ALLOC`. That design makes host-side allocation pressure a
first-class suspect, and it makes the failure cheap to characterise.

**No coverage denominator.** The durable plan is explicit that generated and
semantically verified functions must never share a denominator, and must be
reported separately. Neither register exists, so neither number can be stated.

## 3. Proposed re-sequencing

### Phase 1 — characterise the `bad_alloc` before touching codegen

Cheap, bounded, and it decides everything after it:

1. Re-run the failing ReXGlue invocation under memory instrumentation and
   record peak RSS and the allocation size at failure.
2. Re-run with the host memory ceiling raised, unchanged inputs otherwise.
3. Re-run the **cycle-301** input, which is known good, under the same
   instrumentation, to establish whether peak memory differs materially from
   the cycle-302 input.
4. Read upstream `rexglue/rexglue-sdk` issues and release notes for this
   failure mode.

Exit criterion: the failure is classified as **host resource exhaustion**,
**SDK defect**, or **AC6 codegen defect**, with the measurement that decides
it. Only the third justifies reopening the boundary work.

### Phase 2 — pin ReXGlue explicitly and track upstream

Record the exact ReXGlue commit AC6 builds against, in the workspace, as a
qualified identity like any other input. Decide deliberately whether to keep
consuming it through `AC6_recomp` or to vendor upstream directly. Either is
defensible; the current state — an unrecorded transitive pin — is not.

### Phase 3 — the two registers the durable plan already demands

Generate `functions.csv` over the 23 321 generated implementations, classified
`recompiler-generated`, `probe-covered`, `semantically-verified`, `blocked` or
`open`, plus `subsystems.csv`. Report the two denominators separately, as the
durable plan requires.

Exit criterion: the question "how much of AC6 is done" has two stated numbers
instead of none.

### Phase 4 — resume boundary removal, ranked by execution

Resume `0x8234530C -> 0x8234524C` and later entries only once the runtime is
stable and the register exists, and rank candidates by observed reachability
from the bounded retail smoke rather than by address order.

## 4. Tooling debt

- Add `-Werror` and a sanitizer option, matching Ski Park Manager's CMake.
  AC6 currently has zero warnings, so `-Werror` costs nothing today and
  protects the state.
- `.tools` holds two near-identical clones of the AC6_recomp reference
  (`ac6-recomp-reference` 2.0 GB, `ac6-recomp-main-reference` 1.2 GB) plus a
  1.5 GB `recomp-eval` output tree. Worth a scope decision.

## 5. What this plan does not change

Generated output is never edited. Xenia stays an oracle and never a shipped
dependency. `recompiler-generated` is never `verified`. No retail asset is
versioned or redistributed. No commit or push without an explicit request. The
durable workstreams and gates stand.

## 6. Stated risk

Phase 1 may show the `bad_alloc` is an AC6 codegen defect after all, in which
case cycle 302's original reading was right and one tranche was spent
confirming it. That is an acceptable price: the alternative is resuming
codegen against a runtime whose failure mode is unexplained, which is how a
`runtime_blocked` state becomes permanent.
