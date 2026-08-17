# `ac6-demo-native`

This is an import-only, `supported=false` content boundary for the qualified
PAL demo identity `ac6-demo-xbox360-pal`. It has no scene, simulation, or
interactive product claim.

The executable exposes exactly two commands:

```text
ac6-demo-native import <source-directory> [--store <store-directory>]
ac6-demo-native verify [--store <store-directory>]
```

Without `--store`, data is kept below the product-specific XDG data location
`ac6-demo-native`; the corresponding product-specific XDG config location is
available through the library API. The store contains only an atomic
generation pointer, a private marker, and the nine imported content files.
The marker/profile metadata is never part of the `game:/` VFS namespace.

The installed JSON profile is sealed documentation of the same compiled
identity. It is not read as guest content and cannot be selected by the CLI.

Publication is anchored to POSIX directory descriptors. Imports hold an
interprocess lock, retain the staging/generation and pointer descriptors, and
use a cryptographic ownership token plus device/inode checks. Failed cleanup
first quarantines an object and removes only the expected entries; ambiguous
identity or unexpected entries leave an orphan and fail closed. A same-UID,
non-cooperating process can bypass advisory locks on POSIX; that residual threat
cannot be eliminated without filesystem isolation, so swaps are detected after
rename and never cause an unvalidated object to become `current`. A crash or
ambiguous cleanup may intentionally leave an ownership/quarantine orphan for
later operator recovery; it is never recursively deleted by name.
