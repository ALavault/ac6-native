# `ac6-demo-native`

This is an import-only, `supported=false` content boundary for the qualified
PAL demo identity `ac6-demo-xbox360-pal`. It has no scene, simulation, or
interactive product claim.

The executable exposes exactly two commands:

```text
ac6-demo-native import <source-directory> [--store <store-directory>]
ac6-demo-native verify [--store <store-directory>]
```

For the owned MCP session seam it also accepts the server-supplied private
socket only:

```text
ac6-demo-native --emu-agent-ipc-fd <inherited-fd>
```

The IPC protocol is length-prefixed canonical JSON with bounded typed XInput,
deterministic platform ticks, and tick/PRESENT acknowledgements. The process
never opens a host controller, wall clock, network socket, arbitrary guest
memory, or caller-selected command. Its transport is a platform-only contract:
the MCP observation domains remain `unavailable` until qualified guest
producers and readback evidence exist.

Without `--store`, data is kept below the product-specific XDG data location
`ac6-demo-native`; the corresponding product-specific XDG config location is
available through the library API. The store contains only an atomic
generation pointer, a private marker, and the nine imported content files.
The marker/profile metadata is never part of the `game:/` VFS namespace.

The installed JSON profile is sealed documentation of the same compiled
identity. It is not read as guest content and cannot be selected by the CLI.

Publication is anchored to POSIX directory descriptors. Imports hold an
interprocess lock, retain the staging/generation and pointer descriptors, hash
the complete generation before publication, and recross names with device/inode
identities after validation. Failed publication closes descriptors and leaves
staging, pointer, rollback-link, or generation objects orphaned;
no automatic cleanup or quarantine is attempted. A same-UID,
non-cooperating process can bypass advisory locks on POSIX; that residual threat
cannot be eliminated without filesystem isolation, so swaps are detected after
rename and never cause an unvalidated object to become `current`. A crash or
ambiguous publication may intentionally leave an orphan for later offline,
identity-aware recovery; it is never recursively deleted by name.

The library also contains the first domain-2 boundary: a deterministic 60 Hz
tick counter, typed bounded XInput state, and guest-notified PRESENT count.
It reads no host controller or wall clock. The CLI IPC seam exposes only this
bounded platform contract; it does not establish guest execution, frontend,
mission, renderer, or support. Its in-memory `AC6RTPLY-v4` journal
is exact-PAL, content-hashed, bounded, canonical, and replayed through the same
platform API; no filesystem replay command or guest runtime is exposed.
