# AC6 recursive FHM asset manifest

Date: 2026-07-15.

The native `ac6-asset-manifest` tool decoded all 926 retail table entries and
walked every nested `FHM ` member with a hard maximum depth of 16. It writes no
member payload. Each CSV row contains only the top-level entry, dotted member
path, depth, bounded offset/size, the two still-neutral metadata words, an
exact signature class and at most four prefix bytes in hexadecimal.

Executed result:

```text
entries=926 rows=56514 nested_fhm=5435
empty=11204 fhm=5435 riff=546 ntxr=8006 ndxr=2228 nfic=1549
scene=1293 nfh=1029 mate=733 capt=301 swg=150 ace6=128 nsxr=51
binary=23861
```

The subsequent NTXR structural gate classified those 8,006 records as 7,993
complete wrappers plus 13 explicit 16-byte header references. See
`NTXR_STRUCTURE_REPORT.md`.

Artifact:

```text
reports/fhm-asset-manifest.csv
SHA-256 e77a6e897a9be68b29dbc391e24119121b9958cad5c13230ebdd580fec334cfa
```

A second complete retail execution produced a byte-identical CSV and summary
(`cmp` success), confirming deterministic ordering and output.

The class names are signature transcriptions, not final semantic names. In
particular this pass does not yet claim that every `NTXR` is a directly usable
texture or that every `NDXR` is a complete model. Those claims require bounded
format parsers and full-corpus validation in later passes.
