# AC6 frame-record owner and accessor family

Date: 2026-07-17

## Scope

This static pass follows the owner/accessor helpers used to build the frame
record consumed by the entry-9 unit traversal. It does not assign aircraft,
flight, camera or spawn semantics and does not require a Xenia or human run.

Target: Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

The corrected Ghidra project was queried read-only with `DumpRange.java` for
`0x82097180..0x82097680`, using Ghidra 12.1.2 and the repository's XEX loader.

## Owner initialization

The helper family around `0x82097318` initializes one owner object and leaves a
stable nested source pointer at `owner + 0x264` for later accessors.

`0x82097318`:

- installs a vtable-like pointer at `owner + 0x00`;
- initializes a subobject at `owner + 0x300` through `0x82269218`;
- writes repeated callback/table pointers at `+0x2cc`, `+0x2d8`, `+0x2e4`
  and `+0x2f8`;
- fills a bounded 24-entry pointer region from `+0x70` through `+0x1e0`
  with a common table value.

The preceding helpers at `0x82097190` and `0x820971d8` install adjacent
table pointers and optionally call `0x82383670` when the high input flag is
set. They establish construction/configuration paths but do not expose a
mission or unit identity.

`0x82097480` calls `0x82097318(owner, flags)` and has the same optional
allocator/configuration call. `0x82097470` writes the literal `0x0e` to
`owner + 0x260`; this is an observed owner state value, not a semantic enum.

## Nested source accessors

All five helpers first read `owner + 0x264` and fail closed on a null nested
pointer. Their exact return contracts are:

| Address | Access | Return |
| --- | --- | --- |
| `0x820973a0` | nested `+0x08` then record `+0x00` | 16-bit value |
| `0x820973d0` | nested `+0x0c` then record `+0x00` | byte |
| `0x82097400` | nested `+0x18` | pointer |
| `0x82097420` | nested `+0x08` | pointer |
| `0x82097440` | nested `+0x18` then record `+0x00` | byte |

The frame traversal uses the corresponding source-record helpers before it
reads the dispatch byte at record `+0x2c`. This pass therefore closes the
owner-to-accessor storage boundary, but not the meaning of that byte.

## Candidate dispatch-table initializer checked

The corrected project also contains `0x820ab808`, a virtual method referenced
from data slot `0x820570ac`. Its bounded body writes a table of pointer/value
pairs from `+0x04` through `+0xb0`; one row contains the literal word `6` at
`+0x2c`. This is a useful negative check, not a semantic join:

- the method has a vtable/data reference rather than a direct caller from the
  entry-9 loader;
- its table base is distinct from the source pointer reached through
  `owner + 0x264`;
- no direct reference ties this table to `0x82097400`, `0x820973d0` or the
  frame-record array at context `+0x58`.

Therefore the literal `6` at this candidate table must not be promoted to the
frame dispatch case `0x8222ccd0`. It remains an unrelated or unresolved
table-initialization observation until a binary-qualified object join exists.

## Adjacent initializer family

`0x820974d0..0x82097540` initializes a related object with tables at `+0x348`,
`+0x378`, `+0x38c`, `+0x3a0` and `+0x3b4`, then returns through
`0x82097318`. The object also exposes:

- `0x82097548`: returns `2` when byte `+0x360` is nonzero, otherwise `1`;
- `0x82097560`: returns the word at `+0x364`.

These are bounded state accessors. No direct executable edge ties them to the
entry-9 factory result or to a playable-aircraft selector.

## Native decision

No new native flight or unit helper is added. The reusable contract is limited
to:

```text
owner
  -> nested source pointer at +0x264
  -> source records at +0x08/+0x0c/+0x18
  -> frame-record dispatch byte at +0x2c
  -> bounded traversal cases already documented in cycle 100
```

The following remain open and are explicitly not inferred:

- owner class or RTTI identity;
- source-record semantic names;
- dispatch-byte-to-gameplay mapping;
- relation to player/AI aircraft or camera state;
- initial pose and post-CUT flight activation.

The dynamic owner-table limit from cycle 105 remains valid. A future qualified
savestate or Xenia trace can fill that gap, but no human action is required for
this static tranche.

## Validation

- Read-only Ghidra headless `DumpRange.java` completed for the bounded range.
- No generated output, source, retail asset or emulator state changed.
- No GUI, Wine, VNC, PCSX2, Xenia or human session was started.
- AC6 native CTest remains **41/41** from the current gate.
