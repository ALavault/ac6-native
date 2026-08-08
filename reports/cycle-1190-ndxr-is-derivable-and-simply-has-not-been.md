# Cycle 1190 — NDXR is derivable and simply has not been

## The asymmetry

Cycle 1189 found that the product's NDXR header layout is measured and unaudited,
and called it a bigger open item than any texture question. It is, and it is also
a much easier one than the FHM item beside it.

| | NDXR | FHM |
|---|---|---|
| magic built in the image | **yes** — `0x8233EF48`: `lis r10,0x4e44` / `ori r10,r10,0x5852` | no; the only `0x4648` hits are address arithmetic |
| version extractor | `0x8234CA28`, returns `(byte+0x08 << 8) + byte+0x09` | — |
| typed dispatcher | `0x8234CB58` | — |
| consumer classes, named by J2 | `NU::Model::ModelInfo20Copy` `0x8201283C`, `…NoCopy` `0x820128B4` | none in the class map |
| the parse call | `0x82352B88(this, record)`, from both constructors | — |

Every link of the NDXR chain is already located, from cycle 1150's work on the
resource dispatch. Nothing was blocking it; nobody had followed it, because that
cycle was chasing textures and NDXR was a signpost on the way past.

## What this makes of the two open items

**FHM** is blocked in the way cycle 1175 described: no magic, no named class, a
runtime-populated registry, and the reader still unfound. It needs the delegated
search now in flight.

**NDXR** is not blocked at all. `0x82352B88` is the parse both `ModelInfo20`
constructors call with the record, and reading it would turn
`native_geometry_raster.cpp`'s header offsets from measured into derived —
retroactively auditing a decoder that has been in the product since long before
this session.

That reordering matters. I have spent this session refusing to import measured
formats while an unaudited one sat in the product, and the cheaper of the two
routes to fixing that was already mapped.

## Decided rather than asked

Not read on this cycle, and not for the usual reason. `0x82352B88` is a format
parse — the kind of read where an off-by-one in a field offset produces a decoder
that works on 292 files and fails on the 293rd. That wants a fresh context, and
recorded as task 2g rather than started at the end of a long one.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  26/26 (1 skipped, no DISPLAY)
three live contracts                                ->  audit-valid
```

No product code changed.
