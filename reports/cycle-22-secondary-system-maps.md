# Cycle 22 — AC6 first slot target cross-references

The first fixed pointer written by `0x821d0cf8`, `0x82758e38`, has one writer
reference at the known slot store and several parameter/data references in
other regions (`0x820f83a8`, `0x820f85c0`, and `0x8228e4f0` among them). The
query does not establish a dereference, ownership, queue capacity, or runtime
ordering for any of these references.

This narrows the next static/dynamic investigation to concrete addresses while
leaving the slot system unnamed. Xenia/XenonTests remains required before any
consumer or modding-seam claim.
