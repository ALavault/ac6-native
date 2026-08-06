# Cycle 626 — byte-qualified save-selector helper repair

## Checkpoint

The canonical Ghidra project now contains reproducible function boundaries and
defined instructions for the four save/file-selector helpers that were still
two-instruction imports.  All evidence below is qualified by:

- project: `ghidra-projects/ace-combat-6`;
- program: `default.xex`;
- PAL XEX SHA-256: `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- repair script SHA-256: `02fb56240a2e49f8841328a8ed70bf6417572f50a248044e837c2812e9cfaeac`.

No generated C++ was edited.

## Qualified ranges

The ranges were first read byte-by-byte from the imported program and guarded
with SHA-256 before repair.  The final PPC instruction starts are the words
immediately before the next generated entry point:

| entry | range | bytes | SHA-256 | defined instructions |
|---|---|---:|---|---:|
| `FileSelector_821C3BE8` | `0x821C3BE8..0x821C402C` | 1096 | `0bcfd5aaa6e05a42b6ee68c39d1598c211c9f0630b8499aa6e24b0c1c9efd605` | 261 |
| `FileCreateDialog_821C4FA0` | `0x821C4FA0..0x821C5254` | 696 | `e07514643c0f9d24d4197b0613298a65757406436b424452dbdacf6e22cb11c5` | 164 |
| `FileCreateTask_821C5258` | `0x821C5258..0x821C56F4` | 1184 | `fc64454a236a7892efbadf43250ab6f5a2fd3c15368cc2fd4ce9329f272dfbaf` | 284 |
| `FileCreateState_821C56F8` | `0x821C56F8..0x821C59AC` | 696 | `2340a5aab20b3603c13596b173b9e69b9f7358b73a47ee9482d0480247804f49` | 167 |

The repair script now also marks the `__savegprlr_19` helper at `0x82382ED4`
as returning and clears the false terminal flow on each helper's prologue
call.  Without that correction the function names were present but the
decompiler still returned only the save prologue.

## Direct Ghidra contracts

The repaired decompilation gives the following state-machine evidence.

- `FileSelector_821C3BE8` initializes the selector and dispatches states
  `0..11`.  State `3` reads `[screen+0x10]` as the selected-file result.  A
  positive value sets dialog type `6` at `[screen+0x1c]` and moves to selector
  state `4`; it does not itself provide the dialog response.  State `4` reads
  `[screen+0x0c]` and waits when that response is zero.  States `5/6` then run
  the asynchronous create/load task; success can lead to selector state `10`
  and dialog type `9`.
- `FileCreateDialog_821C4FA0` dispatches create states `0..8`, with dialog
  types `0x25` (37), `0x24` (36), `0x0A` (10), `0x0B` (11), `0x1E` (30),
  `0x1F` (31), `0x22` (34), and `0x23` (35) on their distinct branches.
- `FileCreateTask_821C5258` polls `FileCreateState_821C59B0`.  Its state 2/3/4
  paths distinguish a non-zero result (`[self+0x24]`) from an empty result;
  the non-zero path sets selector state `9` and later dialog types `0x1D` or
  `0x21`.  The zero-result path continues through the asynchronous states
  instead of being an input edge failure.
- `FileCreateState_821C56F8` dispatches states `0..5`.  Its state 3 is the
  only response consumer: it reads `[self+0x0c]`, returns `1` for response
  `1`, and clears/returns `0` for other responses.  States `0..2` and `4..5`
  are storage/task waits, not selector navigation.

These contracts explain why a selector edge and a type-6 dialog response must
be treated as separate guest epochs.  They do not yet prove that the current
type-9 transition is an input-bridge defect: the type-9 branch is also the
qualified path for a non-zero asynchronous load result, and cycles 624/625
used a profile whose files were newly created or not accepted by the load
task.

## Validation

- headless repair: exit 0; log SHA-256
  `2548a14b5b4d8598d21ec58c81dee228a82872bfb62b947a77f65a7fb89afba0`;
- headless instruction/decompilation export: exit 0; direct decompilation log
  SHA-256 `681a3a98cda4d2dbaf0cbe772527ca23ba50ce2e53b00451ddd7cf86c4c52e65`;
- fresh bridge export was run after the final `__savegprlr_19` repair; all
  8,827 functions completed (export log SHA-256
  `1d41d6bd97d9cc97e46c811ae4630fcd34b16383913047bcd68c8257c972f25d`). The
  refreshed selector, create-dialog, create-task and create-state exports are
  retained under `reports/ghidra-cycle-626-*-bridge-v2.txt`;
- `git diff --check` passes for the workspace.

## Next runtime decision

Do not force a type-10 transition.  First replay the state-gated save route
with a qualified, known-good profile (or capture the task result at
`FileCreateTask_821C5258`/`FileCreateState_821C56F8`).  If the guest still
advances to type 9 while `[screen+0x0c]` remains zero, add a serial/epoch guard
around the native edge latch; otherwise treat the transition as the load-task
contract and pursue save-file validity.
