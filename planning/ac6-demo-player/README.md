# AC6 demo player — isolated scaffolding

Status: `scaffold-only`, not wired into a product, test target, MCP server or
handoff.

This directory prepares independent contracts for the next AC6 demo work.  It
must not be used as parity evidence and deliberately has no CMake, runtime or
MCP integration.

| Area | Scaffold | Integration boundary |
|---|---|---|
| acceptance | `acceptance-matrix.md` | durable evidence and canonical gates |
| START A/B | `start-ab-reducer.py` | qualified trace reader and first-divergence receipt |
| reference FSM | `fsm-reference-v1.json` | `emu_step` observations and controller gains |
| native identity | `demo-native-identity-profile-v1.json` | native import/store policy |
| receipts | `receipt-verifier.py` | schema registry and content-addressed store |
| observations | `observation-domain-inventory-v1.json` | runtime telemetry producers |

Promotion rule: no scaffold field may be interpreted as available telemetry,
game semantics, product support or an endogenous milestone.
