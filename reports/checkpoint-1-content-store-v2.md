# Checkpoint 1 — content-store v2 progress

Date: 2026-08-11

## Delivered

`RetailContentStore` now writes index version 2 (224-byte header) and embeds a
content-addressed media-manifest digest. The six PAL compressed media assets are
qualified by exact size and SHA-256, copied in bounded streaming chunks, and
published through `media/current` only after their manifest and blobs exist.
`RetailMediaStore` validates the manifest, every blob digest, and bounded range
reads. A v1 `current` pointer is rejected as `cache_incompatible`, requiring an
explicit re-import.

The product import command retains `--frontend` for compatibility but seals all
926 DATA.TBL entries in one generation. The importer default now means the
complete table closure; bounded fixture callers can still pass an explicit
entry span.

## Qualified local smoke import

Source: `game-files`, PAL `default.xex` and DATA.TBL/PAC identities from
`RetailIdentityPolicy::pal()`. The temporary cache was imported with:

```sh
ac6-native import --source game-files --cache /tmp/ac6-retail-v2-smoke
```

Observed result:

```text
records=926 bytes=5424368676
index_sha256=cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85
media_manifest_sha256=43abbdb317d1c2d202a9df81b42657be750f31eaf37d8cac76ac738b476f24d7
payload_blobs=816 media_blobs=6
```

The index and media manifest are both independently content-addressed. The
complete expanded DATA.TBL closure is about 5.42 GiB; the per-payload cap stays
512 MiB and the generation cap is 8 GiB.

## Validation

- `ac6-retail-content-tests`: pass, including media publication,
  reproducibility, range bounds, blob corruption, and v1 rejection.
- Full PAL smoke import: pass, 926 records and all six media files.

The resource graph is now materialized before the index pointer is published.
The smoke cache contains 46,236 nodes rooted at all 926 entries, with 6,361
FHM containers, 1,549 NFIC nodes, 1,293 Scene nodes, 150 SWG nodes, 733 MATE
nodes, 2,228 NDXR nodes, 8,006 NTXR nodes, and 7,493 non-zero GIDX keys. Every
node carries its parent, child slot, depth, type, and relative byte range; NTXR
nodes carry the qualified GIDX identifier. The graph also contains 55,743
NDXR material-to-texture relations; all 55,743 texture IDs resolve to an NTXR
GIDX node in the qualified corpus. Empty retail FHM sentinels are
recorded as leaves and never assigned invented child semantics.

## Open boundary

The graph now closes structural resource identity, parent/child/GIDX links, and
the NDXR material-to-texture registry relation. Semantic material parameter
decoding and language/media cue decoding remain checkpoint 2/6 work; they are
deliberately not claimed by this import gate.
