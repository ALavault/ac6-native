# Cycle 548 — atoi correction and Ghidra-bridge reconciliation

Date: 2026-08-02

## Qualified target

- project: `ghidra-projects/ace-combat-6`
- module: PAL `default.xex`
- SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- bridge: local `ghidra-bridge` with Ghidra 12.1.2, fresh export of 8,825 functions

## Proven correction

Cycle 546 observed the generated public thunk at `0x82382480` return 22 for
valid decimal inputs such as `"200"`.  The canonical bridge independently
confirms the literal PAL thunk:

```text
82382480 li r5,0xa
82382484 li r4,0x0
82382488 b 0x823821d0
```

The native override now implements only this public `atoi` contract with a
bounded guest-string read and deterministic decimal conversion.  The general
locale-aware `strtol` worker remains untouched.  The build succeeds and all
seven AC6 tests pass, including boundary and saturation cases.

## Runtime replay

Both force options remained false.  The historical recipe no longer observed
its expected `type28=8` after the save-file confirmation.  A bounded follow-up
proved the route was on the Game Data creation UI: `type28` moved `6 -> 9 -> 5`.
The first capture shows the `Create new Game Data?` dialog; the second shows the
three empty file slots after confirmation.  This is an earlier route timing /
state-predicate issue, not evidence that the loadout transition regressed.

- binary SHA-256: `3661c96b44d0f77bb0c7a111ba7b2f81ce14705a3c8a54e681f22e3c53ea4c66`
- runtime log SHA-256: `73e12da52334bccf89e14a52d5764a14a8c0e07b6b4a0c9cbd4a846a2a43d18f`
- dialog capture SHA-256: `851dc9a06c8578a110513ac7cc35a33df101dfc285e017f48a95140009e4c57a`
- file-list capture SHA-256: `689fbcae5ebc9afa2517ca76e8b0825b914cb1cb22f57f74838891036d0cc576`

## Reconciliation boundary

The fresh bridge export exposes a material contradiction with cycle-544's
generated-control-flow interpretation:

- canonical Ghidra contains `Function_820F6228` through `0x820F62A8`; address
  `0x820F62B0` is not a function and is reported as offset `+0x88` in that
  function's containing range;
- canonical `Function_820F6330` is only the thunk `bl 0x82382EFC`; its exported
  CFG is one block ending at `0x820F6334`;
- the revision-pinned generated corpus instead configures starts at
  `0x820F62B0` and `0x820F6330` and emits the `Mddd` parser/broadcaster bodies;
- canonical Ghidra also has no function start at `0x8214D390`.

Therefore the cycle-544 names and semantics for these configured generated
starts are not canonical Ghidra evidence and must not guide another fix until
the byte/function-boundary discrepancy is reconciled.  The `0x82382480` thunk
is cross-matched by both sources and remains qualified.

## Next checkpoint

First make the Game Data recipe state-based enough to reach the loadout again
with the `atoi` correction.  In parallel, compare bounded canonical program
bytes at `0x820F6228..0x820F63C0` and `0x8214D360..0x8214D430` against the
revision-pinned generated instruction corpus and its original analysis input.
Do not let configured generated starts override canonical Ghidra boundaries.

## Cycle 549 correction

The apparent byte/function discrepancy above was resolved. SHA-guarded
headless dumps from canonical `ace-combat-6` match the revision-pinned generated
instruction corpus exactly. The missing/truncated catalog entries came from a
false no-return helper annotation and absent function boundaries, not different
program bytes. `RepairQualifiedFunctionBoundaries.java` persistently repairs
the canonical project; the parser at `0x820F62B0`, `SendMsgV` at `0x820F6330`
and dispatcher at `0x8214D390` are now canonical evidence. The warning against
blindly trusting configured starts remains valid, but these three starts are no
longer quarantined.
