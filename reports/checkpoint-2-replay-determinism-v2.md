# Checkpoint 2 — replay identity v2

`RetailSessionReplay` is now version 2.  The file records the mission,
difficulty, validated aircraft/weapon loadout, cache index SHA-256 and the
60-Hz input frames.  `replay` passes the stored difficulty into
`RetailMissionBundle`; it cannot silently replay at another difficulty or
against another cache.

The reader accepts version-1 files and migrates their absent difficulty to
`Normal` before validation.  Writes remain atomic through the existing
temporary-file/rename path.  Unit coverage includes current-format round-trip,
bad magic, invalid identity, and a hand-built v1 migration.

This closes replay option identity, not campaign checkpoint persistence or a
complete final campaign digest; those remain open in the global ladder.
