# Cycles 629–630 — read-only save epoch boundary

Target: PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
Runtime SHA-256:
`4ec87f9646a2de14a786fb8afabf61721d6ce7b34a96a0abe6c172912f4fac2b`.
Lane: `observe`; startup intervention manifest: none.

## Result

- Cycle 629 proves `FileCreateTask_821C5258` transitions state `3 -> 9`,
  result `0 -> 3`, and type `0 -> 30`. One A edge naturally publishes
  response 2; `FileCreateState_821C56F8` returns 1 and the outer path advances
  through type 37. The save modal does not require response synthesis.
- Cycle 630 naturally reaches selector state 4/type 6 after selecting a file
  slot. On the following post-call snapshot it is already state 10/type 9 with
  response 0. The external harness cannot inject between those two calls.
- The current boundary is therefore a same-edge/in-call epoch problem, not the
  older request-armer chain and not a renderer failure.

## Validation

- `reports/logs/cycle-629-save-modal-observe/ac6recomp.log`
- `reports/logs/cycle-630-first-mission-observe/ac6recomp.log`
- cycle 629 captures hash-qualified in the run directory.

## Next checkpoint

Capture the response field before and after each in-call producer/consumer in
`FileSelector_821C3BE8`, correlated with the edge serial. Require proof of the
exact writer before introducing an epoch guard. Do not force type 10, restore
loadout force flags, or use a bridge run as natural evidence.
