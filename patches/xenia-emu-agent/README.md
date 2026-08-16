# Xenia `emu-agent/v1` bridge patch

This directory contains a reproducible, AC6-independent patch for the pinned
Xenia Edge source checkout. It adds a disabled-by-default `AgentBridge` seam:
actions are bounded by completed `XE_SWAP` presents and are consumed by
controller slot zero. It does **not** provide a transport, pause protocol,
guest memory/register reads, save states, or framebuffer capture.

Base checkout:

- source: `.tools/xenia-edge-source`
- commit: `e4b13738c3c461b2c06241fa3f54b5a669b6a304`
- AppImage oracle: release `60ff861`, SHA-256
  `c2cac2a029ce0d44a71c4e919fd71c702654079023b63fd669472ba3cd78b828`

The working Xenia checkout is intentionally not modified by the AC6 backend.
The patch is only an integration artifact and must be applied to a disposable
copy. It is not a claim of retail/native parity.

## Apply and verify

```sh
./patches/xenia-emu-agent/apply-and-verify.sh /path/to/xenia-edge-source-copy
```

The script refuses a wrong base commit, a dirty worktree, or an already
applied patch. Build and test using Xenia's own documented commands. The
resulting bridge remains disabled unless the Xenia HID flag `--agent_bridge`
is explicitly enabled by a future, separately qualified transport.

## Boundary and claims

The patch supplies only these generic Xenia observations:

- a monotonic completed-present counter after `IssueSwap`;
- a present-bounded controller action API with strictly increasing IDs;
- input substitution for guest controller slot zero while an action is held.

The current AC6 backend therefore reports `diagnostic` and
`needs-dynamic-evidence`; it cannot submit an action or publish a safe-point
observation until a local authenticated transport is added and tested.
