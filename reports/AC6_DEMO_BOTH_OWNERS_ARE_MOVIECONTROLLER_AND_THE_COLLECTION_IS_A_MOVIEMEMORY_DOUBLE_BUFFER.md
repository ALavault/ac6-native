# Both owners RTTI-resolve to `swg::MovieController`; the "collection" is a `swg::MovieMemory` double-buffer, not a plain vector

## Qualification

AC6 demo PAL, same XEX SHA-256. Live evidence: three probe runs, each the
existing `AC6_DEMO_WATCH_ADDR_LO/HI` bracket (no source change) pointed at
one live heap object's own header — the two owner addresses `bfc927e1`
found (`0x2E3EDA90`, `0x2E4035D0`) and the collection pointer read off the
first owner's own `+16` field (`0x2E3EDCD0`) — `probe --until frontend
--max-ticks 3200`, correctly-timed START, no oracle. Static: RTTI walks
(`vtable-4` → locator → `locator+12` → type descriptor `+8`) against
`.build/Default.xex.base.bin`, by hand, same method `whose_vtable.py`
uses internally; a raw dump correcting the opcode table's stub count.

## What this corrects

**`bfc927e1`'s stub-index list contains a fabricated entry, and its count
was drawn from an incomplete scan.** That report said the jump table's
"generic/error/no-op handler... fills at least ten other slots... indices
`0xB, 0xF, 0x10, 0x14, 0x1C, 0x1D, 0x1E, 0x23, 0x24, 0x26, 0x28`" — eleven
values captioned as ten, and one of them (`0x28`) is wrong: index `0x28`
holds a real handler, `0x82324638`, not the stub. The underlying script
that produced the list only ever dumped indices `0`-`39`; nothing in that
report scanned the table's full extent (`0`-`103`). A full re-scan finds
**23** stub-mapped indices, not ten:

```
0xB, 0xF, 0x10, 0x14, 0x1C, 0x1D, 0x1E, 0x23, 0x24, 0x26,
0x2A, 0x2B, 0x2C, 0x2D, 0x33, 0x56, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x63, 0x64
```

`bfc927e1`'s underlying claim — opcode `0xB` dispatches to a one-instruction
`blr` stub, structurally explaining `4ee47a17`'s "not found" result — is
unaffected; the miscount is corrected here because a wrong number left
uncorrected in this campaign's own reports is exactly the failure mode
`CLAUDE.md` names by number (cycles 1133/1134).

## Both owners are `swg::MovieController`

`bfc927e1` tracked the interpreter slot's `+4` ("owner") field through
26+ distinct values without reading any of them. Bracketing the two most
active owners' own headers — the press/resume owner `0x2E3EDA90` and the
dominant per-tick owner `0x2E4035D0` — catches each one's construction
directly (`sub_82323808`, tick 2451 and 2571 respectively) and both write
the *same* vtable at `+0`:

```
locator = [vtable - 4] = 0x8206D94C  (for 0x820304D8, both instances)
type_descriptor = [locator + 12] = 0x8264CE30
name = [type_descriptor + 8] = ".?AVMovieController@swg@@"
```

**`swg::MovieController`, confirmed live on two real instances** — the
same class `6d61b5cd` named from a purely static reading of
`sub_82323BB8`'s own RTTI neighbour. This is independent confirmation,
not a new claim: two different heap objects, two different construction
ticks, the same vtable, the same name.

Both instances' full 8-word header (offsets in decimal, matching this
campaign's existing convention):

| offset | owner `0x2E3EDA90` (tick 2451) | owner `0x2E4035D0` (tick 2571) |
|---|---|---|
| `+0` (vtable) | `0x820304D8` | `0x820304D8` |
| `+4` | `0x0` | `0x0` |
| `+8` | `0x0` | `0x0` |
| `+12` | `0x2E3E3AD4` | `0x2E3E3AD4` |
| `+16` (collection) | `0x2E3EDCD0` | `0x2E403810` |
| `+20` | `0x0` | **`0x2E3EDA90`** |
| `+24` | `0x0` | `0x0` |
| `+28` | `0x0` | `0x0` |

`+12` is the same constant on both instances, and matches the *interpreter*
execution-context object's own `+0` vtable value from `bfc927e1`
(`0x2E3E3AD4`) — not a pointer to an instance, the shared vtable constant
itself, stored as data. `+16` differs between the two instances: **each
`MovieController` owns its own, distinct collection object** — not a
single shared queue, correcting the implicit "the collection" framing in
`390cfe33`/`6d61b5cd` to "each instance's own collection." `+20` is new:
the second-constructed instance's `+20` field holds the *first*
instance's own address — a same-class link, read but not yet identified
as parent/sibling/free-list (see Not established).

## The "collection" is `swg::MovieMemory<stx::lwallocator<unsigned char,0,64>>`, not a plain vector

Bracketing the first owner's own collection pointer (`0x2E3EDCD0`) catches
its construction (`sub_820D0DB8`, tick 2451, a different constructor from
`MovieController`'s own) and its vtable resolves cleanly:

```
locator = [0x8200654C - 4] = 0x8206D94C   // note: same locator address
                                          // family as MovieController's,
                                          // different content at +12
type_descriptor = [locator + 12] = 0x82385A58
name = [type_descriptor + 8] =
    ".?AV?$MovieMemory@V?$lwallocator@E$0A@$0EA@@stx@@@swg@@"
```

`swg::MovieMemory<stx::lwallocator<unsigned char, 0, 64>>` — the same
allocator template `346255b2` already read off `ASContext::String`'s own
mangled name, now on a class named for memory management rather than a
generic container. `6d61b5cd`'s "sized, indexable collection... `Count()`/
`GetAt(index)`" description was accurate at the interface level; the class
underneath is not a generic vector, it is (by name) a movie-clip-specific
memory manager, and its layout backs that up:

| offset | value at construction (tick 2451) |
|---|---|
| `+0` (vtable) | `0x8200654C` |
| `+4` | `0x2E3DF9D0` (unidentified — see Not established) |
| `+8` | `0x5A` (90, a plain integer — capacity? unconfirmed) |
| `+12` | `0x2E3EDD54` (buffer A) |
| `+16` | `0x2E3EDF94` (buffer B) |
| `+20` | `0x2E3EDD54` (= buffer A, duplicate of `+12`) |
| `+24` | `0x2E3EDF94` (= buffer B, duplicate of `+16`) |
| `+28` | `0x2E3EE1D4` (a third pointer, unread) |

`+12`/`+16` and `+20`/`+24` start out as duplicate pairs — the shape of a
double buffer, one "current" pair and one "next" pair pointing at the same
two underlying buffers. Once per tick, `sub_82323BB8` (the same per-tick
driver `6d61b5cd` already read) makes one virtual call on this object
before its queue-drain loop — `[[this+16]+0]+4` (vtable slot 1, `this` =
the `MovieMemory` object) — and only `+20`/`+24` change afterward,
alternating between the two buffer addresses each time it's observed
(ticks 2452 and 2453 in this run). Read plainly: this looks like a
**publish/swap step for a producer/consumer double buffer** — the pair a
writer is filling gets swapped with the pair the tick's own drain reads
from — but no writer side has been located, and "looks like" is as far as
this report takes it.

## What this means for "who enqueues"

`6d61b5cd` framed the open question as "who calls `Add`/`Insert` on the
collection." This report doesn't answer that, but it changes the shape of
the answer: there is no single shared collection to instrument — each
`MovieController` has its own `MovieMemory`, and that object's own
`Count`/`GetAt` interface sits on top of what looks like a double-buffered
producer/consumer structure, swapped once per tick by a call this report
found but did not follow. The natural next read is that one virtual call
(vtable slot 1 on `MovieMemory`) in full, plus whatever writes into the
buffer pair *between* swaps — the writer side, still not located.

## Not established

- `MovieMemory+4` (`0x2E3DF9D0` on the one instance read) and `+28`
  (`0x2E3EE1D4`) — not read.
- Whether `+8`'s value (`0x5A` = 90) is a capacity, an element count, or
  something else — not confirmed against any bound check.
- `MovieMemory`'s vtable slot 1 (the per-tick swap call) — its body not
  read; "looks like a buffer publish/swap" is a reading of the write
  pattern, not a disassembly.
- The writer side of the double buffer — nothing in this report locates
  what writes new offsets into the buffer pair between swaps, which is
  where "who enqueues, and could it ever enqueue EndMode's offset" is
  actually answered.
- `MovieController+20`'s link between the two owner instances (second →
  first) — read as a raw pointer match, not traced to a specific field
  role (sibling list? parent? free chain?).
- Whether more than two `MovieController` instances exist beyond the 26+
  values `bfc927e1` already logged as owners of the shared interpreter
  slot — this report only resolved the two most active ones.

## Gates

No source changed; three live runs reused the existing
`AC6_DEMO_WATCH_ADDR_LO/HI` instrument at different addresses, plus static
RTTI reads. Native gate JF, demo `ctest` (26/26), and both contract audits
verified below before commit.
