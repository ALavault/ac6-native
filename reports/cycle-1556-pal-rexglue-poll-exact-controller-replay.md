# PAL ReXGlue poll-exact controller replay

Date: 2026-08-12

## Result

An unsealed prototype patch for detached `AC6_recomp` `dcd41b` replaces the
manager-tick TSV injector with a bounded `ac6.controller-input-replay.v1`
service at the actual `XamInputGetState_entry` seam. It records or replays raw
XAM arguments, caller LR, pointer nullness, result and controller state. Poll
order is the primary synchronization key.

The `0x821CA908` PAL function entry is wrapped by a handwritten strong symbol.
It emits the `ac6_frame_input_stage` marker before guest instructions and then
calls the generated implementation. The service starts after XEX loading and
before guest launch; it finalizes only after the guest thread has joined.
Record output is bounded, self-validated, published without overwrite, and
replay requires exact EOF. Any identity, event-order, guard, limit, corruption
or truncation disagreement fails closed.

No generated source is part of the prototype. The patch carries the handwritten
configuration start for `0x821CA908`; generated output is recreated only in a
disposable worktree. The stack auditor now rejects patch targets containing a
`generated` component or named `ppc_recomp.*`.

## Reproduction identities

- prototype patch: 14 handwritten paths, SHA-256
  `623ffecb806f1e062dec248bfe5c11da7c16f726ef1022f275ee583a3433dad8`
- base: the separately sealed 13-patch capture worktree; the prototype is not
  listed by `patches/stack.json` and does not alter its identities
- prototype capture configuration:
  `d2d149db190eb4952e5008f9181080af5f4c49f35ad0547c4eaca206a5b4bc6b`
- generated result: 10,479 functions, 56 files, 104,738,935 bytes,
  SHA-256 `028ffdacccb5ee38c6feee483d0f0ca9b941e464ea64b37aadb023e590df4755`
- Release binary: 50,903,256 bytes, SHA-256
  `70387538d1224118c2f20607a1fc2845c8dad164a08a1effa2a25739cc04a4d9`

The final build used system Clang 21, `-march=x86-64-v3`, the qualified SIMDe
overlay and the preset's `-O3 -DNDEBUG`. All four host CTests passed. The linked
binary contains strong `rex_sub_821CA908`, `__imp__rex_sub_821CA908`,
`XamInputGetState_entry`, replay lifecycle and non-advancing guest-clock peek
symbols. Application and `git diff --check` also passed in a fresh detached
disposable worktree.

## Evidence boundary

This is compilation and unit-test evidence for the seam only, not integration
or qualification of a new stack revision. No interactive oracle was launched.
The sealed stack remains at 13 patches and its reproducibility contract is
unchanged. The prototype raw v1 header, including its fixed `native_hz=60`
shape, is incompatible with the controller reader currently under revision;
it must not be used as the future wire contract.

- marker cadence and phase frequency have not been measured;
- end-to-end record, replay and first-divergence behavior have not run;
- replayed polls are consumed but a simultaneous observed-poll comparison
  trace is not emitted yet;
- capabilities and keystroke XAM calls remain outside this first tranche;
- the harness supplies the canonical identity header; runtime self-attestation
  of every header digest remains open;
- a runtime census must still confirm cross-thread poll/marker ordering.

Consequently cadence remains `unqualified`, resampling remains `refuse`, gate
evidence is false, and no capture or projection is authorized. After the raw
v3 contract is frozen, this prototype is to be replaced incrementally with
that schema plus the runtime census; none of the current reader's provisional
v2 fields are copied into this patch.
