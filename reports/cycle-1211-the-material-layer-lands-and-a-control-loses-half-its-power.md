# Cycle 1211 — the material layer lands in the product, and a control loses half its power on the other corpus

## What was added

`NdxrContainer` gains the material and texture layer derived in cycle 1207:

- `Material(record, descriptor_index, slot)` — `0x82355468` relocates
  `sub+0x10..+0x1C`; this resolves instead. Shader id at `+0x00`, texture count
  at `+0x0A`, resolve bit `0x4000` at `+0x08`.
- `TextureRef(material, index)` — `0x82355318`: records of stride `0x18` from
  `material+0x20`; the **texture id** is the `u32` at `+0x00`, which is the key
  into registry `0x828C8100` that `0x8234BEC8` fills from NTXR packs, keyed by
  `GIDX+0x08`.
- `ParameterChainLength(material)` — `0x82355394`: nodes carrying their own byte
  size at `+0x00`, zero terminating.

**This closes the derived path onto code the product already has.** The texture
id is the key; `ntxr_texture.h` already decodes what the key names. Nothing is
wired to the renderer, because the vertex data is still not addressed by anything
on the derived path.

Over the corpus: 537 files, 13,014 records, **13,014 materials, 13,014 texture
references**, every resolve bit clear on disk, every parameter chain terminating.

## The control lost half its power, and I did not weaken the test to hide it

Cycle 1207's discriminating table killed the rival strides `0x10` and `0x20` at
**0 of 1,227**. Re-expressed against the standalone corpus, `0x10` still dies at
**0 of 13,014** — and **`0x20` terminates 13,014 times out of 13,014.**

The reason is arithmetic and it is the far-side rule again. The parameter chain
starts at `material + 0x20 + count * stride`, so the rivals are separated by
`count × (stride difference)`. The two corpora differ exactly there:

| corpus | `texture_count == 1` | `== 2` |
|---|---|---|
| standalone, 537 files | **13,014** | 0 |
| MDLP, 292 NDXR | 293 | **934** |

With `count == 1`, stride `0x20` sits eight bytes from `0x18` and lands on a
valid chain as well. With `count == 2` it is sixteen bytes off and dies. **Cycle
1207's control was run on the only corpus where it works.**

The test now asserts what this corpus can actually discriminate — `0x10` must
fail — reports `0x20` without asserting it, and **asserts the count census
instead**, so that the day a material with `count > 1` enters this corpus the
exemption announces itself as stale rather than quietly persisting.

Weakening the assertion until it passed was the available alternative. It would
have turned a control into decoration.

## Why this keeps happening, stated once

Four times now: cycles 1202, 1203, 1209 and this one. Every instance is a
measurement that is **correct on the data it was taken from** and does not
generalise, and every instance was caught by running it against the other side.
The rule in `INSTRUMENT_DISCIPLINE.md` is holding up, and its cost is roughly one
extra query per claim — which is cheap against the four cycles of published
error it has now caught.

Worth noting what it is *not*: none of these was a careless read. The
disassembly was right every time. What failed was the inference from a
population to a rule.

## Not established, stated plainly

- The vertex data. Still nothing on the derived path addresses it, and it remains
  JV's blocker.
- Slots 2–4 of the four material pointers, null in all 13,014 cases here and in
  all 1,227 there — untested, not confirmed absent.
- The parameter chain's contents. The walk is derived; the nodes' meaning is not.
- The 77 references and 124 textures cycle 1210 left unmatched.

## Verification

```
ndxr-container files=537 opened=537 refused=0 records=13014
  materials=13014 textures=13014
  resolve bits set on disk                : 0 / 0
  parameter chain terminates, stride 0x18 : 13014 of 13014
  materials with texture_count == 1       : 13014 of 13014
  rival stride 0x10 / 0x20                : 0 / 13014
ctest -> 27 tests, all passed (1 skipped, no DISPLAY)
audit --require JF -> mission01_final_gate=audit-valid JF=pass open=none
```
