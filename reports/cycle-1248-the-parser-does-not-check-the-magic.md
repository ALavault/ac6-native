# Cycle 1248 — the parser does not check the magic, and cycle 1246's premise was wrong

## The correction, and it is to a decision I made two hours ago

Cycle 1192 scanned the loaded image for the bytes `46 48 4D` and found **zero**,
with three positive controls. That measurement is correct and stands.

**The inference from it was wrong.** Cycle 1192 concluded, and cycles 1240 and
1246 carried, that *retail does not parse an FHM container*. What is actually
true is narrower: **the parser does not check the magic.**

`0x82234C18` is a directory reader, read here in full:

```
82234c18  lbz r11,0x4(r4)     ; version
82234c20  lbz r11,0x5(r4)     ; endian marker
82234c28  lhz r11,0x6(r4)     ; the table offset
82234c34  cmpwi cr6,r10,0x1   ; endian != 1 -> byte-swap everything
82234c40..58                  ; the swap
```

No magic comparison anywhere. It takes a blob, reads a version byte, an endian
byte and a table offset, and walks. **A format is parsed; its name is never
looked at.** That is exactly why a byte scan for the tag returns zero, and why
that zero was misread.

## The control, on my own corpus

If `0x82234C18` is what reads FHM bundles, `u16[blob+0x06]` must be a real,
uniform property of them:

| extension | n | `u16[+0x06]` |
|---|---|---|
| `.fhm` | 439 | **16 (0x10), all 439** |
| `.ntxr` | 1052 | 1, all 1052 |
| `.mdlp` | 3 | 94, all 3 |
| `.ndxr` | 537 | scattered — 3241, 4297, 7497, … |

`0x10` puts the count at `0x10` and the offset table at `0x14` — **exactly where
`tools/ac6_fhm.py` reads them and where cycle 1192 measured the layout on 94 of
94 bundles.** The rival *"any file in this family has 0x10 there"* is dead by the
other three rows.

**Two independent derivations of the same header now meet**: one measured from
the bundles, one read out of the image. The layout cycle 1192 called
*"measured, not derived"* is derivable after all — from a function that never
mentions FHM.

## What this does to cycle 1246

**The decision stands. The reason does not.**

Cycle 1246 decided the product must not get an FHM reader, and gave as its ground
that porting one would be *"porting something the shipped game does not do"*.
The game does do it. The decision survives on the remaining grounds — the
product's boundary is where `NdxrContainer` already sits, and retail resolves
resources by id — but **a decision resting partly on a false premise is one I
have to say so about**, and the report is corrected in place.

## And the practical answer is better than I said

Cycle 1246 called the id path *"harder than the task implied"* because the
registries are BSS, populated at runtime. Measured now:

- **`[0x828C9700+0x08]`, the id rebasing bias, is zero.** Written exactly once, a
  literal zero in the constructor `0x82340A60`, and the address is *materialised*
  at `82335f2c` rather than inferred from stride arithmetic — closing cycle
  1209's open item and correcting its provenance.
- So **an offline index can key exactly as retail does**, and cycle 1208's worry
  about pack-local rebasing — already retracted in cycle 1209 — is now closed
  from the writer's side too.
- The six `mode = 0` mount sites take each entry's own GIDX id, and every
  extracted `.ntxr` is a **self-describing pack of count 1**, so on that path
  **pack grouping is irrelevant**: feeding the 1052 files one at a time to a port
  of `0x82340870` yields the same 205 keys.

**JV 2a is less blocked than cycle 1246 concluded.**

## Three more corrections owed by name

- **Cycle 1201** — the registry's element pool is `0x140` bytes, not `0xC0`. The
  `0xC0` belongs to `0x8234CC88`, a different class. Three derived texture
  classes share the `0x140` block, so it is a maximum, not any one class's size.
- **Cycle 1207** — *"takes each entry's GIDX identifier, failing hard when there
  is none"* holds only for `mode & 1 == 0`. The other arm never calls
  `0x8234B150`.
- **Cycle 1209's layout of the value object is confirmed in full** at `+0x08`,
  `+0x0C`, `+0x10`, `+0x14`, `+0x1C`, `+0x50`; only its object size was wrong.

## Not established, stated plainly

- **Duplicate-mount policy.** `0x8234BEC8` looks up before creating, so the first
  mount of an id wins. Over 1052 packs and 205 ids that is 847 duplicates, and
  which payload retail ends up holding depends on **mount order, which the flat
  extraction does not record.**
- **The `mode = 1` packs.** Their ids are `base + index within the pack`, and the
  extraction flattens membership, so those ids are not recoverable from the
  corpus even though the bases are literals.
- **Pixels are located, not decoded.** Level-0 data lands at file offset 4096 in
  1052 of 1052. `remaining / (w × bpp × h)` clusters at 0.25, 1.0 and 4.0 —
  consistent with block compression and **not** with a naive linear reading.

  > **Correction, same session, by the cycle that wrote it.** This bullet
  > continued: "converting it needs `0x821FCA48`, the X360 tiler, unported and
  > with an unestablished contract." **That is false, and it was false when
  > written.** `src/ntxr_texture.cpp` already untiles Xenos `Tiled2D`
  > (`xenos_tiled_2d_offset`, `pad_to_tile` at 32 blocks) and decodes BC1/BC2/BC3
  > under contract behaviour `texture_decode` — 692 wrappers, 668 decoded
  > (656 BC3, 10 BC1, 2 BC2), 22 refused not-block-format, 2 refused cube-map,
  > corpus pixel hash `8a7b59cbf13ba39b`, endianness control 468/170/30.
  >
  > The sentence came from a delegated investigation. It was true of what that
  > agent had been shown and false of the repository, and I carried it into a
  > report, a task list and `MISSION01_LADDER.md` without one `grep`. See the
  > twentieth shape in `INSTRUMENT_DISCIPLINE.md`: *an agent's scope, written as
  > the repository's*. What is still open here is the **binding** — which
  > wrapper a material's texture id resolves to — not the decode.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
0x82234C18 read in full here; u16[+0x06] censused over 2,031 extracted files
```

No product code changed.
