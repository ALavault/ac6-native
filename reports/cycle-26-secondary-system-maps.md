# Cycle 26 — AC6 post-slot critical section

Read-only Ghidra verification confirms that the post-slot service
`Function_82332318` enters a critical section, calls `Function_8233b790`, then
leaves it.  This is static sequencing and synchronization structure only; it
does not name the slot list or prove a Xenia execution path.
