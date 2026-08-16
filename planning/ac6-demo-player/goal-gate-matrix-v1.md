# AC6 demo player — goal gate matrix v1

Target: `ac6-demo-xbox360-pal`, Xbox 360 Xenon/PAL demo, `Default.xex`.
Overall support remains `supported=false`.

The authoritative anchor is [`CURRENT.json`](../../../../reports/handoff/CURRENT.json)
(SHA-256 `79dd3a…4618d7a`), cycle 1760
([JSON](../../reports/cycle-1760-ac6-demo-event-handoff.json), SHA-256
`6258f9…68884af`). Statuses are strict: `proven`, `partial`,
`failed`, or `missing`. Green unit/CTest results are never runtime parity.

| Gate | Status | Evidence and limit |
|---|---|---|
| Identity/XEX/basefile/Ghidra | proven | `config/demo-identity.json`; XEX `de917873…405da8`, basefile `b98a9ac1…14218`, Ghidra manifest `576fa31e…0086c`. Identity is not support. |
| Negative identity/corpus | partial | Nine-file profile and rejection list exist; content test rejects empty store, but full retail/truncation/tamper/symlink corpus is not receipted. |
| Codegen boundary | proven | Codegen manifest `9f1fffb0…5bbfa`: 0 boundary diagnostics, 0 unsupported instructions; job `msw5b55a` rc=0 and CTest 20/20. Third-party compiler warnings remain in the job log. |
| Two-codegen reproducibility | proven | Cycle 1759 receipt `4e992d4b…f99af`: manifests/generated/object trees equal. This is not semantic parity. |
| Install/no `bin/bin` | proven | Cycle 1759 install PASS; canonical `bin/ac6-demo-recomp` exists and both `bin/bin` paths are absent. |
| Final package hygiene | missing | No final `ac6-demo-native` package receipt; XEX/PAC/TBL/SDK/Ghidra/generated-C++ exclusion gate remains open. |
| Guest writer → PM4 → RB_COPY | proven | Cycle 1754 receipt joins writers `0x821B5840`/`0x821B7C04` to IB offsets 239/387; two cold artifacts identical. |
| Graphics/Xenos coverage | partial | Cycle 1755: 921600/921600 MSAA samples, but all resolved pixels are zero. |
| Non-black readback | failed | `resolved_rgba8_sha256=0b150fd3…ec58366`, 230400 black pixels, no sentinel/other pixels. |
| Kernel/XAM event transport | partial | Cycle 1760: 962 set→wake→resume chains on `0xE000004C`; focused trace capped at 32768 and only covers to tick 1212. |
| VFS/user store | partial | Static store/VFS implementation and empty-store test exist; no complete nine-file guest mount receipt. |
| Time/scheduler | partial | 5600 ticks, 5463 PRESENT, 23 blocked/0 runnable; 60 Hz simulation/30 Hz presentation cadence not proven. |
| Guest memory/post-resume | partial | Earlier capsule has 351 same-tick memory rows; current bounded event capsule observes no post-resume access. |
| VMX/CPU runtime | partial | Bounded codegen/CPU route reaches max ticks without unsupported instruction/fault; full VMX semantic gate absent. |
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
| demo-native domain 1 | partial | Identity/import handoff is exact but explicitly `import-only`/`not-wired`; no `reconstruction/ac6-demo-native`. |
| demo-native runtime domains | missing | Player/camera/flight/target/objective/terminal/readback inventory all unavailable. |
| Two cold success/failure runs | missing | Current cold A/B both return `max_ticks`, rc=4; no success and distinct failure terminal. |

## Single next checkpoint

Run one fresh neutral/buttons=16 A/B `post_resume_one_shot` capsule at the same
PAL identity and 5600-tick bound, adding
`AC6_DEMO_WATCH_POST_RESUME_ACCESS=1`. Use the existing one-shot probe and
`map_generated_guest_load_sites.py`; do not modify generated C++.

| Branch | Status | Classification |
|---|---|---|
| Exactly one `load*`/`store*` access | proven | Map generated function/line to PAL PC/bytes. This closes only the event-access observation; graphics/readback stays open. |
| Explicit `AC6_POST_RESUME_ACCESS_REFUSED` | partial | Record a refusal boundary; do not infer scalar memory semantics or promote support. |
| No capture, malformed, multiple rows, or bound ends first | missing | Bounded no-capture only; it is not proof that the guest access is absent. |

The machine-readable canonical record is
[`goal-gate-matrix-v1.json`](goal-gate-matrix-v1.json).
