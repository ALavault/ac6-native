# Cycle 16 — AC6 following task-loop phase

`0x821d7c80` follows `0x821d7a90` in the persistent task-loop body. It checks
`DAT_826e4e58 == 1`, writes two mode/configuration values, performs several
named direct calls and two virtual dispatches, then returns to the caller's
back-edge. The mode source and virtual targets are not dynamically observed.

This distinguishes the following phase from the reset/service phase without
claiming a title, graphics, audio or gameplay role.
