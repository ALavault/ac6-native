# frame-loop-probe nt-ordered-release — cycle 323

Runtime measurement of the NT-ordered-release fix, binary
`13a9f256ff3400d8c4d17a471b020a49f2fc6709e27b50ee2f284804f0a752a9`.

`guest.log` is **0 lines, and that is the finding**, not a collection failure:
this run predates cycle 324, so the probe did not yet pass
`--ac6_performance_mode=false`, and the runtime had its own log level forced to
`error`. The guest-state samples and thread dumps are valid — they come from the
shm reader and `/proc`, neither of which depends on the runtime's logging.

Read `PRESENT 0 / VdSwap 0` in the cycle 323 report as "not counted", never as
"zero frames". Cycle 324 explains why and fixes the probe.
