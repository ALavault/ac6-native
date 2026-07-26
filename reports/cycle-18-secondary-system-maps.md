# Cycle 18 — AC6 slot-helper no-return inconsistency

The task-loop slot helper `0x821d0cf8` conditionally calls `0x821d0c58`, but
the latter is presently a Ghidra no-return wrapper to `0x82382efc` while the
caller has subsequent code. This is a concrete ABI/function-boundary
inconsistency, not evidence of an actual terminal path. It blocks direct
recompilation classification for this conditional branch and requires raw PPC
instruction/ABI verification before semantic naming.
