# AC6 cycle 215 — fixed-offset overflow rejection for motion records

Target: `ac6-xbox360-pal`; module `default.xex`; SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

## Change

The bounded type-`0x8181` views now reject 32-bit guest-address overflow for
every fixed field access as well as for relative relocations:

- `record + 0x0a`, `+0x10`, `+0x14`, `+0x18`, and `+0x1c`;
- `entry + 0x10` in both materialization and the `0x8211bd50` reader.

Previously, a high guest record or entry could wrap a fixed offset into low
guest memory before the range check.  Two regressions forge precisely those
wrapped address shapes and require a clean rejection.

## Boundary

This is a host-safety rule for malformed input.  It neither claims that a
retail Xbox 360 fault is handled identically nor assigns motion, aircraft,
camera, or mission semantics to tag `0x8181`.

## Validation

- `ac6-motion-record-tests` PASS;
- complete AC6 CTest corpus: **42/42 PASS**;
- root-prefix install with no `bin/bin`;
- `git diff --check` PASS.

No Xenia, generated XenonRecomp output, GUI or human interaction was used.
