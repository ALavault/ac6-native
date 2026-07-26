# AC6 cycle 214 — fail-closed `0x8181` motion-record materialization

## Scope

Target: `ac6-xbox360-pal`; module: `default.xex`; SHA-256:
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

This tightens the already bounded type-`0x8181` native portion of retail
`0x82118a50`. It adds no tag, consumer, object or movement semantics.

## Native safety boundary

Before relocating any `0x20`-stride entry, the native model now validates:

- `record + relative_table` without 32-bit guest-address overflow;
- every entry address and its `+0x10` word;
- every `record + entry_relative_word` relocation without overflow.

Only after the complete preflight does it write relocated words, the table
field and the materialized bit. A malformed/truncated guest range therefore
returns no result without modifying a prefix of the caller-supplied host span.
This is a bounded-host-safety property, not a claim that a malformed retail
record has identical fault behaviour on Xbox 360.

## Validation

The dedicated test now covers normal materialization, repeat materialization,
wrong tag, truncation, a valid first entry followed by a truncated second entry
with byte-for-byte no mutation, and overflow of the relative table address.
The complete AC6 CTest corpus is run after this change. No Xenia, GUI,
generated XenonRecomp output, retail asset or human interaction is used.
