# Cycle 1200 — the streams resolve by id, against two registries

## The three calls that were left unread

`0x8233EE40`, called per element of the `0x18`-byte array:

```
8233ee54  lhz  r11,0xa(r31)              ; flags = elem+0x0A
8233ee58  rlwinm. r11,r11,0x0,0x11,0x11  ; bit 0x4000 — already resolved?
8233ee5c  bne  0x8233eeac                ;   then skip
8233ee64  lwz  r4,0x0(r31)               ; key = elem+0x00
8233ee60  lis  r11,-0x7d73
8233ee6c  subi r3,r11,0x7f00             ; registry 0x828C8100
8233ee70  bl   0x8233ebb0                ; lookup(registry, key, &out)
8233ee7c  li   r3,0x0                    ; not found -> fail
```

`0x8233EF88`, called on the stream descriptor at the end:

```
8233ef9c  lhz  r11,0x8(r31)              ; flags = stream+0x08
8233efa0  rlwinm. r11,r11,0x0,0x11,0x11  ; the same 0x4000 bit
8233efac  lwz  r4,0x0(r31)               ; key = stream+0x00
8233efa8  lis  r11,-0x7d73
8233efb4  subi r3,r11,0x3480             ; registry 0x828CCB80
8233efb8  bl   0x8233f2b0
```

So the geometry does **not** carry its resources inline. Both the stream and each
element of its array are **references resolved by integer id**, against **two
different registries** — `0x828C8100` and `0x828CCB80` — with a second guard bit,
`0x4000`, distinct from the `0x8000` relocation guard of cycles 1197–1199.

Two guards, two meanings: `0x8000` says "pointers have been rebased",
`0x4000` says "ids have been resolved". A port that conflates them will
double-resolve on a second visit.

`0x82362190`, called once at the top of `0x823556E0`, reads `[arg+0x10]`, compares
it to 1 and sets a local to 4, then walks from `arg+0x14`. It is not a file
reader and I did not follow it further.

## The control

Both guards must be **clear on disk**, since both are set by the loader:

| prediction | result |
|---|---|
| `stream+0x08` bit `0x4000` clear | **13,014 / 13,014** |
| `elem+0x0A` bit `0x4000` clear | **13,014 / 13,014** |
| `stream+0x08` top two bits both clear | **13,014 / 13,014** |

The third is the one I did not have to run and am glad I did: it shows `0x8000`
is clear there too, so the same word carries both guards and both start unset.
Had either bit been set in the shipped files, the field reading would be wrong.

## Two things the census says that the disassembly did not

**Only one of the four stream slots is used.** Cycle 1198 derived four pointers
at `sub+0x10..+0x1C`. Walking them across the corpus yields **13,014 non-null
descriptors for 13,014 sub-records** — one apiece. Three of the four slots are
empty in all Mission 01 content. The format has four; the content uses one.

**There are 179 distinct ids** at `elem+0x00` across the whole corpus, the
commonest appearing 519 times.

That number is also, separately, the count of distinct NDXR models recorded
elsewhere in `MISSION01_LADDER.md`. **I am not concluding they are the same 179.**
Two counts landing on the same integer is the shape of coincidence this
repository has been burned by — cycle 1198's `0x118` quotient was exact for two
thirds of a corpus and meant nothing. The control that would settle it is to
resolve an id through `0x828C8100` and see what comes back, which requires
reading `0x8233EBB0` and finding what fills the registry; neither is done.

## Not established, stated plainly

- `0x8233EBB0` and `0x8233F2B0`, the two lookup functions, and what populates
  either registry. Without that, "resolved by id" names a mechanism and not a
  destination.
- Which resource kind each registry holds. Two registries and two call sites is
  a structure, not a meaning.
- Whether the 179 ids are models, materials, textures or something else.
- The vertex stride `0x14` remains derived but uncontrolled (cycle 1198).

## Where this leaves the port

The NDXR chain is closed as a *container* walk: recognise, dispatch, construct,
sequence, header, records, streams, names — every stage derived, every stage with
a control. It is **not** closed as a *geometry* reader, because the vertices are
behind an id resolution whose far side is unread. Writing the container walk into
the product now would be honest; calling it a mesh loader would not.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
13,014 / 13,014 on all three guard predictions
```

No product code changed.
