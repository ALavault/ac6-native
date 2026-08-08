# Cycle 1249 — four container readers, and which of them check a magic

Cycle 1248 established that `0x82234C18` reads the FHM layout without comparing a
magic. That leaves an implicit question a future reader would get wrong: **is it
*the* container reader?** It is not. There are at least four, and they disagree
about the same bytes.

## The header bytes, censused over the extracted corpus

`0x82234C18`'s first two tests are the byte at `+0x04` (version) and the byte at
`+0x05` (endian marker; anything but 1 triggers a byte-swap):

| extension | n | `(+0x04, +0x05)` |
|---|---|---|
| `.fhm` | 439 | **(1, 1)** — all 439 |
| `.mdlp` | 3 | (0, 0) — all 3 |
| `.ntxr` | 1052 | **(2, 12)** — all 1052 |
| `.ndxr` | 537 | (0, 0) ×459, (0, 1) ×42, (0, 2) ×30 |

Only `.fhm` passes `0x82234C18`'s endian test cleanly. Run against an MDLP the
same reader would **byte-swap everything** and take `u16[+0x06] = 94` as a table
offset — when 94 is the entry *count*:

```
0x82234C18 would read : version 0, endian 0 -> BYTE-SWAP; "table offset" 94
0x8228E9B8 does read  : count u32[+0x04] = 94; table u32[+0x0C] = 0x1000;
                                               base  u32[+0x10] = 0x2000
```

Cycle 1157 measured that MDLP layout on the file and cycle 1174 ported it from
`0x8228E9B8`. **The two readers are different functions reading different
formats**, and the coincidence that both find 94 near `+0x04` is exactly the kind
of near-fit this campaign has been burned by three times.

## Four readers, and the structural fact

| reader | container | checks a magic? |
|---|---|---|
| `0x82234C18` | FHM | **no** — version, endian, table offset |
| `0x8228E9B8` | MDLP | **no** — count, offset table, base |
| `0x8234B300` | NTXR | **yes** — `'NTXR'`, then version ∈ {1,2} and subtype |
| `0x8234CA28` | NDXR | **yes** — `'NDXR'`/`'GIDX'`/`'NUP3'`, then the `u16` code |

**The packaging layers are magic-free; the payload layers are magic-checked.**
That is a coherent design — a container is opened because the caller already
knows what it holds, while a payload is dispatched on what it says it is — and it
explains cycle 1192's zero completely rather than by exception.

The `.ntxr` census also confirms, on my own corpus, the pair `0x8234B300`
accepts: **version 2 with subtype `0x0C`, in 1052 of 1052.**

## What this changes

Nothing already published, and one thing that was about to go wrong. Cycle 1248
could be read as *"`0x82234C18` is how retail opens containers"*, and a port
built on that would byte-swap every MDLP it touched. The reader-to-format map
above is the correction, made before anyone acted on it.

## Not established, stated plainly

- Whether `0x82234C18` has other callers reading other formats. Its argument is a
  blob pointer supplied by the caller, and I did not enumerate them; the map
  above is *which reader reads the formats in this corpus*, not *what each reader
  is used for*.
- The `.ndxr` `(+0x04, +0x05)` spread — (0,0), (0,1), (0,2) — is the file's own
  size field's high bytes, not a version pair. It appears here only to show that
  the byte census discriminates, and no claim is made about it.
- Whether any format uses the endian-swap path. All 439 FHM are `(1, 1)`, so on
  this content the swap at `82234c40` never runs — another live-but-unused branch,
  the fifth this session.

## Verification

```
ctest --test-dir reconstruction/ace-combat-6/build   ->  27 tests, all passed (1 skipped)
audit ... --require JF                               ->  mission01_final_gate=audit-valid JF=pass open=none
2,031 extracted files censused on bytes +0x04 and +0x05
```

No product code changed.
