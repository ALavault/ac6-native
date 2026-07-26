# Cycle 15 — AC6 task-loop phase map

The persistent synchronized task handoff calls `0x821d7a90` before
`0x821d7c80` on its back-edge loop. Static export evidence shows that
`0x821d7a90` performs several virtual calls, clears a bounded group of global
state fields, and conditionally calls a service sequence based on a pointer
root and its fields. `0x821d7c80` remains a separate following phase.

This is a binary-qualified loop-phase map only. No dynamic Xenia observation
yet connects either phase to title boot, game simulation, graphics, or audio.
