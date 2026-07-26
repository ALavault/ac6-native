# Cycle 21 — AC6 pointer-array reference bound

The read-only reference query for global array base `0x829cba08` finds five
data references at `0x821d0d90`, `0x821d0da8`, `0x821d0dc4`, `0x821d0de4` and
`0x821d0dfc`, plus one parameter reference. These are the five fixed stores in
the already mapped `0x821d0cf8` region; no direct consumer reference appears
in this query.

This turns the unknown consumer into a precise static boundary: the array is
written at five sites, but capacity, indirect consumption and system role are
not established. The next evidence remains Xenia/XenonTests observation, not
a semantic name.
