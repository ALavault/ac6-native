# AC6 PAL demo completion matrix — scaffold

Target: `ac6-demo-xbox360-pal`, `Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.

This is a planning matrix, not a gate receipt.

| Gate | Current evidence class | Missing proof | Canonical validation placeholder |
|---|---|---|---|
| identity/content | partial-qualified | native profile and negative corpus | `TODO(identity-gate)` |
| codegen/CPU | partial-qualified | two clean identical codegens after final patch stack | `TODO(codegen-gate)` |
| kernel/XAM/VFS | reached-partial | frontend-producing service closure | `TODO(runtime-gate)` |
| Xenos/Vulkan | neutral-qualified | first guest-driven non-black readback | `TODO(renderer-gate)` |
| input/time/replay | input-delivered | persistent START-owned guest transition | `TODO(replay-gate)` |
| XMA/media | reached-experimental | qualified effects and media completion | `TODO(media-gate)` |
| frontend | unavailable | persistent guest milestone plus readback | `TODO(frontend-gate)` |
| mission/objectives | unavailable | positive and negative endogenous terminals | `TODO(mission-gate)` |
| MCP v2 | scaffold/under-review | owned runtime transport and episode receipt | `TODO(mcp-gate)` |
| reference FSM | scaffold-only | qualified observations and deterministic route | `TODO(fsm-gate)` |
| demo-native | unavailable | separate executable and domain parity | `TODO(native-gate)` |
| final cold runs | unavailable | two identical successes and one distinct failure | `TODO(final-gate)` |

The authoritative status remains the project receipts and handoff; this file
must be updated only when a cited gate receipt exists.
