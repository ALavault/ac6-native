# Cycle 1266 — a declaration cache, and a `.pdata` row I read as a dispatch slot

## Qualification

`ghidra-projects-xenon/ac6-xenon`; `.pdata` and data reads directly from
`analysis-input/ACE6_X360.exe`; `default.xex` SHA-256 `acc302c1…11bcde`.
**No oracle pass was spent.** No product code changed.

## Established — `0x82346FC0` builds a declaration cache, from `0x821DE898`

Cycle 1265 left the `+0x28` pair unread. It is the more interesting of the two:

```
82346fd0  or   r27,r3,r3
82346fd8  lis  r11,-0x7d99
82346fe0  addi r28,r11,0x6520   ; a table at 0x82676520
82346fe4  or   r31,r28,r28
82346fe8  lwz  r3,0x0(r31)      ; take a pointer from it
82346fec  bl   0x821de898       ; build a declaration from that element list
82346ff0  stw  r3,0x0(r30)      ; store the result
82346ff4  addi r31,r31,0x4      ; next
```

It walks a table of element-list pointers at `0x82676520`, calls the **vertex
declaration allocator** on each, and stores the results. That is a declaration
cache being **built**, and `0x82346F88` is its clear:

```
82346f8c  li     r11,0x19       ; 25 slots
82346f94  stw    r9,0x0(r10)
82346f9c  addi   r10,r10,0x8    ; of 8 bytes
82346fa4  addi   r10,r3,0xc8    ; 25 * 8 = 0xC8, so the second array follows it
82346fa8  li     r11,0x10       ; 16 more
```

Twenty-five slots then sixteen, both of 8 bytes, contiguous.

**This ties `0x821DE898` into the object.** Cycle 1261 filed that address among
ten the `ndxr_container` behaviour declares and nothing implements; cycle 1262
read it and put it with the D3D device. It is both: the device allocates the
declaration, and this object caches twenty-five of them at `+0x28` while holding
the `3 × 6 × 8` tables at `+0x170`.

## Correction — to cycle 1265, by its author, and it is an instrument fault

Cycle 1265 wrote that both sibling functions are *"reached through tables rather
than by call"*, citing that `0x8233E6B0` appears once as aligned data at
`0x82085888` and `0x8233B2E8` once at `0x82085618`.

**Both of those are `.pdata` rows.** The exception table spans
`0x82079E00`…`0x82089FB0`, 8,246 entries of 8 bytes, and the two addresses are
the `BeginAddress` fields of entries 5969 and 5891. The neighbouring words —
`8233e580 40001903 8233e5e8 40002e03 8233e6b0 40001903` — are the alternating
address and packed prolog/length word of a `RUNTIME_FUNCTION`, not a dispatch
table of any kind.

So the sentence carries **no information at all**: a function with an exception
record appears once as data by construction. How `0x8233E6B0` is reached is
**unknown**, which is what cycle 1265 should have said, and its zero direct `bl`
callers remain the only fact about it.

### The instrument, and why one earlier negative survives

The "does this address appear as aligned data" scan has been load-bearing twice
this session. It is safe only when the address has **no `.pdata` row**, and
`.pdata` here is incomplete — 8,246 rows for a program with more functions than
that:

| address | `.pdata` row | so a data hit means |
|---|---|---|
| `0x8234CDC0` (registry insert) | **no** | a real reference |
| `0x82345098` | **no** | a real reference |
| `0x8233E6B0`, `0x8233B2E8`, `0x82345100`, `0x821DE898` | **yes** | nothing, until the row is excluded |

**Cycle 1255's negative stands**: `0x8234CDC0` has no exception record, so its
zero data hits genuinely means no vtable and no dispatch table reaches the
registry insert. That was luck rather than method — the scan did not know the
difference, and neither did I when I ran it.

**The rule: exclude the `.pdata` range before reporting a function address as
data**, and say which side of that line the address falls on.

## Not established

- **The table at `0x82676520`** — its length, and whether the 25 and 16 slots
  correspond to two groups within it. The loop's bound was not read past the
  first iteration.
- **What the class is.** Unchanged and now with one fewer false clue: neither
  function is in a vtable, and nothing here names the type.
- Whether the `+0x28` declaration array and the `+0x170` tables index the same
  way. They are different shapes — 25 + 16 against 3 × 6 × 8 — so probably not,
  and "probably" is doing real work in that sentence.
