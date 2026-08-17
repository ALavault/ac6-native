# Cycle 1753 — garde XMA jointe au contexte physique PAL

La garde opt-in du registre `0x7FEA1A80` exige désormais, en plus de
l'adresse/longueur/valeur wire et de l'ordre des bits, que le dernier
`MmGetPhysicalAddress` porte le contexte attendu. Un run neutral frais confirme
les six couples exacts :

```text
0x2E800000 -> 0x01000000  (tick 1048, bit 0)
0x2E800040 -> 0x02000000  (tick 1048, bit 1)
0x2E800080 -> 0x04000000  (tick 1048, bit 2)
0x2E8000C0 -> 0x08000000  (tick 5052, bit 3)
0x2E800100 -> 0x10000000  (tick 5052, bit 4)
0x2E800140 -> 0x20000000  (tick 5052, bit 5)
```

Après le sixième tuple, le run atteint `max_ticks=5400` : 5263 PRESENT,
`frontend=false`, `mission=false`, `terminal=false`, et 23/23 threads guest
bloqués sur `0x822E559C -> 0x822F8848`. Les deux IB restent
`ef7ab6e4…d2b0`/`d121c8d8…358d6`. Aucun effet XMA, paquet audio ou pixel n'est
matérialisé.

Artefacts :

- rapport : `/fastdata/lavaulta/tmp/ac6-cycle1753-proof.7t16Uy/neutral.report.json`, SHA-256 `ec40217ffd824cd7e6f525497b31a05d4c6ca7e14bfdf2746e8de6fc3ce93a44` ;
- trace : `/fastdata/lavaulta/tmp/ac6-cycle1753-proof.7t16Uy/neutral.trace.jsonl`, SHA-256 `793f57e338aa7265f1e4d3c06d0a6d9865fc34da37e0ec9abee1653d6a5c1d6e` ;
- stderr de provenance : SHA-256 `99f1278ca16212cb9a7e3c25365fc3e9c954ebf55106bb59017e88eb9c9cc1df` ;
- binaire codegen-ON : SHA-256 `b8b155faded9a453ee115c42ee533a29c572b9ca278fe46960f984d5332f9a07`.

La garde ne consulte ni Xenia ni HID et reste désactivée par défaut. Le
prochain checkpoint est toujours le writer EDRAM/source `RB_COPY`, neutral
seulement; START et l'audio restent gelés.
