# AC6 demo player — goal gate matrix v1

Target: `ac6-demo-xbox360-pal`, Xbox 360 Xenon/PAL demo, `Default.xex`.
Overall support remains `supported=false`.

The portfolio handoff anchor remains [`CURRENT.json`](../../../../reports/handoff/CURRENT.json)
(cycle 1761, supported=false). The latest AC6 checkpoint is the domain-1
`ac6-demo-native` receipt
([JSON](../../reports/cycle-1764-ac6-demo-native-domain1-publication.json),
SHA-256 `877ab1a1…adad`), while the post-resume one-shot receipt remains
([JSON](../../reports/cycle-1761-ac6-demo-post-resume-one-shot.json), canonical
SHA-256 `600c40c2…51e07`). Statuses are strict: `proven`, `partial`,
`failed`, or `missing`. Green unit/CTest results are never runtime parity.

| Gate | Status | Evidence and limit |
|---|---|---|
| Identity/XEX/basefile/Ghidra | proven | `config/demo-identity.json`; XEX `de917873…405da8`, basefile `b98a9ac1…14218`, Ghidra manifest `576fa31e…0086c`. Identity is not support. |
| Negative identity/corpus | proven | Cycle 1764 verifies all nine PAL files, `supported=false`, exact sizes/SHA, isolation, multiprocess publication, swaps and targeted rename/fsync rollback. |
| Codegen boundary | proven | Codegen manifest `9f1fffb0…5bbfa`: 0 boundary diagnostics, 0 unsupported instructions; job `msw5b55a` rc=0 and CTest 20/20. Third-party compiler warnings remain in the job log. |
| Two-codegen reproducibility | proven | Cycle 1759 receipt `4e992d4b…f99af`: manifests/generated/object trees equal. This is not semantic parity. |
| Install/no `bin/bin` | proven | Cycles 1759 and 1762 install PASS; `bin/ac6-demo-native` is installed, and `bin/bin` is absent. |
| Final package hygiene | missing | No final `ac6-demo-native` package receipt; XEX/PAC/TBL/SDK/Ghidra/generated-C++ exclusion gate remains open. |
| Guest writer → PM4 → RB_COPY | proven | Cycle 1754 receipt joins writers `0x821B5840`/`0x821B7C04` to IB offsets 239/387; two cold artifacts identical. |
| Graphics/Xenos coverage | partial | Cycle 1755: 921600/921600 MSAA samples, but all resolved pixels are zero. |
| Non-black readback | failed | `resolved_rgba8_sha256=0b150fd3…ec58366`, 230400 black pixels, no sentinel/other pixels. |
| Kernel/XAM event transport | partial | Cycle 1760: 962 set→wake→resume chains on `0xE000004C`; focused trace capped at 32768 and only covers to tick 1212. |
| VFS/user store | proven | Cycle 1764: autonomous CLI/store/VFS, PAL import/verify, descriptor-anchored full-SHA publication and atomic rollback pass in the separate product store. This remains import-only. |
| Time/scheduler | partial | 5600 ticks, 5463 PRESENT, 23 blocked/0 runnable; 60 Hz simulation/30 Hz presentation cadence not proven. |
| Guest memory/post-resume | partial | Cycle 1761 closes only the first-access observation boundary: each fresh route has one handoff and one `load64`, mapped to unique PAL PC `0x82327154` (`0x7F0409D8`, bytes `eb61ffd0`). Semantic role, START consumption and readback remain open. |
| VMX/CPU runtime | partial | Bounded codegen/CPU route reaches max ticks without unsupported instruction/fault; full VMX semantic gate absent. |
| Xenos runtime | partial | Cycle 1754 joins guest writers to PM4/RB_COPY, but cycle 1755 readback remains black; no complete translation/resolve parity is promoted. |
| XMA runtime | partial | Six context-qualified kicks are traversed under opt-in guard; no XMA effect is promoted. |
| Media/audio decode | missing | No decoded packet, timestamp/volume, or English/Japanese audio output receipt. |
| Input delivery | partial | `buttons=16` appears at tick 252/sequence 760; no semantic START transition follows. |
| START transition | failed | Neutral/buttons A/B have identical graphics/scheduler/milestones. |
| Frontend | missing | Both 5600-tick routes report `frontend=false`; no positive milestone/readback pair. |
| Mission | missing | `mission=false`; no title→mission receipt. |
| Objectives | missing | Observation inventory marks objective unavailable; no endogenous producer. |
| Results/terminal | missing | `terminal=false`, outcome only `max_ticks`; no success/failure result. |
| MCP v2 surface | proven | Cycle 1757 receipt `ddbcedb8…de48f8`: six v2 tools, v1 compatibility, 17 tests. Contract only. |
| MCP ownership/lifecycle/replay | proven | Cycle 1759: immutable receipt replay, session ownership/close; 54 tests + 1 skip. Fixture/contract scope only. |
| MCP runtime artifacts | partial | AF_UNIX/Popen transport is implemented/tested, but PAL allowlist is empty and observations remain unavailable. |
| FSM contract | proven | Cycle 1758 receipt `39c186e0…e06c7`: 10 states/9 budgets, deterministic controller, fail-closed guards; 43 tests. |
| FSM PAL route | missing | Scaffold/runtime route has no qualified observations, allowlist, or terminal receipt. |
| demo-native domain 1 | proven | Cycle 1764 validates identity/import/CLI/VFS/PAL corpus/install plus adversarial publication/rollback. It remains `import-only` and `supported=false`. |
| demo-native runtime domains | partial | Cycles 1765–1766 add typed XInput, deterministic ticks/PRESENT and byte-identical in-memory `AC6RTPLY-v4` replay with 5/5 CTest. No IPC, guest scheduler, gameplay observation or runtime route exists. |
| Two cold success/failure runs | missing | Current cold A/B both return `max_ticks`, rc=4; no success and distinct failure terminal. |

## Single next checkpoint

Expose the bounded platform/replay path through an owned `demo-native` IPC
session while returning every guest/gameplay domain as unavailable.
Keep frontend/mission unsupported and keep the cycle-1761 XAM frontier
separate until its A/B receipt is delivered.

| Branch | Status | Classification |
|---|---|---|
| Runtime/domain 2 promotion | partial | Platform/input/time and native replay contracts pass; owned IPC and guest/runtime comparison remain absent. |

The machine-readable canonical record is
[`goal-gate-matrix-v1.json`](goal-gate-matrix-v1.json).
