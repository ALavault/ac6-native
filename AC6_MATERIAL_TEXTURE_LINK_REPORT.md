# AC6 first MDLP/NDXR/MATE/NTXR material-texture join

Date: 2026-07-15 (Europe/Paris)

## Closed identity path

The first two animated aircraft now follow one bounded native identity chain:

```text
NFIC object id -> Scene path/resource MOP -> MDLP model element
  -> NDXR polygon texture-id records -> adjacent MDLP texture element
  -> NTXR GIDX
```

The concrete pairs are model/texture MDLP elements `76/77` for `r_f16c` and
`78/79` for `r_f18f`. This adjacent second-element use agrees with the already
recovered `0x820a7070` path that can register/select one or two MDLP elements.

NDXR polygon descriptors point to bounded texture-info tables. Each table has
a `0x20`-byte header followed by `0x18`-byte records whose first big-endian
word is the texture identifier. Across the two aircraft, 115 polygon texture
references exactly match an NTXR `GIDX` in their paired MDLP bundles.

The same NDXR identifiers have 227 exact big-endian occurrences in the four
MATE payloads belonging to those bundles. The first aircraft MATE header gives
seven materials and 54 batches. Region 0 is a `0x10`-stride material-offset
table; region 1 is a `0x10`-stride batch table whose first word packs the
sequential batch ordinal in the high half and material index in the low half.
The 54 entries match the 54 NDXR polygon descriptors exactly. Material texture
records begin at material offset `+0x20`, stride `0x18`, and are byte-identical
to their NDXR texture records. The second aircraft closes by the same exact
checks. Shader parameters and blend modes remain open.

## Next exact NTXR boundary

The first matched identifier is NDXR `0x10002215`. Its paired NTXR has the same
`GIDX`, a bounded 507,904-byte aggregate texture-data tail and these twelve descriptor
words:

```text
00040050 00000000 00040000 00500000
00010001 02000200 00000000 00000000
00000ff0 00000000 00000000 00000000
```

The earlier single-entry interpretation has been corrected. The wrapper's byte
`0x07` is a count of six `0x50`-byte entries. Each is a `0x30`-byte descriptor,
an `eXt` chunk and a `GIDX`; descriptor word 8 is a relative data offset and
word 2 is the bounded allocation size. For `0x10002215`, word 5 gives
`512x512`, word 0 gives the exact `0x40000 + 0x50` logical entry size, and word
2 allocates exactly `0x40000` bytes.

That first profile is now decoded natively as a single-level BC3 texture using
Xenos tiled-2D block addressing and `8-in-16` endian conversion. The contract
is supported by exact byte consumption and an intelligible retail aircraft
diffuse atlas; the unconverted-endian control is visibly corrupted. No mip
tail is claimed for this entry, and the five sibling profiles are not promoted
to supported formats by analogy.

NDXR polygon indices are local to each descriptor: every non-restart index is
below that polygon's position count (`index_oob=0`), so the per-polygon base
index is exactly zero. Presentation is now guarded by the complete chain:
MATE batch ordinal -> material -> first texture id == NDXR polygon first
texture id == decoded NTXR GIDX, with a complete UV0 stream. This proves 52 of
the first aircraft's 54 batches and 63 of the second aircraft's 63 batches.
The two first-aircraft batches without complete UV0 remain wireframe. Each
object selects only its own decoded NTXR through its own binding contract;
object order is not used as evidence.

## Observable native frame

The current frame presents two partially textured native retail aircraft;
only the two first-aircraft batches without a complete UV0 stream remain
wireframe:

- PNG: `captures/first-native-textured-aircraft-frame.png`
- BMP source emitted directly by SDL3: `captures/first-native-textured-aircraft-frame.bmp`
- PNG SHA-256: `ab836712ab704280acd3ba50a196c5dbb72de46125ba35d92ba5ac97dedadced`
- BMP SHA-256: `39e49d9a6dc9bf295ab85cdb548fb1bcc56aec5e9d9c999fca9b7035292febda`
- decoded BC3 preview: `captures/first-linked-0x10002215-bc3-preview.png`
- raw retained wrapper: `exports/first-linked-0x10002215.ntxr`
- raw wrapper SHA-256: `72ab39e3d4fe0e5085fa94fad6015b55ff843dae5f93fb35028f37b86b446863`
- raw first-aircraft MATE: `exports/first-linked-r_f16c.mate`
- raw MATE SHA-256: `dfe0a409829f9c6bc0a41b52ddeedf041c3b408bc9fb3c61eb43cc8aece4460e`

The frame contains 9,711 retail vertices and 14,492 retail indices, placed from
the first CUT's MOP tracks. It remains conservatively labelled
`world_renderer=native-partial`.

## Executed retail smoke

```json
{"status":"ok","camera_states":120,"world_objects":2,"retail_vertices":9711,"retail_indices":14492,"mate_payloads":4,"ntxr_payloads":4,"mate_texture_links":227,"ndxr_ntxr_links":115,"textured_polygons":115,"index_oob":0,"mate_bound_objects":2,"diffuse_bound_polygons":115,"decoded_texture_gidx_by_object":[268444181,268444652],"diffuse_bound_polygons_by_object":[52,63],"presented_textured_objects":2,"presented_textured_polygons":115,"world_renderer":"native-partial"}
```

The test and presentation gates are GCC 17/17, Clang ASan+UBSan 17/17, plus a
full decoded-entry-9 hidden-window render/capture smoke. Its direct SDL BMP
has SHA-256 `4bc623fbce8689e2e970f986c7019834e8fcc8a3818f86298a6e9de959e1d938`.

## Native batch-binding contract

`resolve_mate_ndxr_batch_texture_bindings` now makes the renderer's repeated
batch-level identity check a fail-closed library operation. It requires one
MATE batch per NDXR polygon, validates the ordinal and material index, and
reports whether their first texture identifiers match. The contract deliberately
does not assign shader semantics or interpret later texture records.

Synthetic matching, mismatch, and cardinality-rejection regressions pass in
the native MATE test. Fresh Linux GCC and Clang ASan/UBSan suites each pass
17/17, including the SDL scene-shell smoke.

The scene shell now stores this resolved binding vector with each joined world
object and uses it for both textured draw eligibility and its
`diffuse_bound_polygons` accounting. It remains fail-closed for missing or
mismatched first identifiers and for non-one-to-one batch/polygon counts. This
is only the established first-texture identity gate: MATE shader parameters,
later texture records, culling, and blend state remain outside the native
partial renderer.

The shell now also uses that same binding vector to choose an NTXR decode:
it no longer picks the first decodable NDXR texture reference. A texture is
selected only after a matching MATE-first and NDXR-first identifier resolves to
an NTXR `GIDX`. This makes the displayed diffuse atlas depend on the complete
proven identity chain rather than on polygon-reference traversal order. The
remaining entries in the retained wrapper are not called a mip chain or used
for rendering without a separate retail consumer proof.
