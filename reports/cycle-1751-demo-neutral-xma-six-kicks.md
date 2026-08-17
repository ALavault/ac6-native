# Cycle 1751 — six kicks XMA neutral traversés, attente scheduler

Le probe neutral headless frais, lancé depuis le store qualifié avec la garde
expérimentale strictement séquentielle, accepte les six tuples observés du
callsite PAL `0x82357240` / `LR=0x823572AC` :

| tick | contexte | valeur wire | bit logique |
|---:|---:|---:|---:|
| 1048 | `0x2E800000` | `0x01000000` | `0x01` |
| 1048 | `0x2E800040` | `0x02000000` | `0x02` |
| 1048 | `0x2E800080` | `0x04000000` | `0x04` |
| 5052 | `0x2E8000C0` | `0x08000000` | `0x08` |
| 5052 | `0x2E800100` | `0x10000000` | `0x10` |
| 5052 | `0x2E800140` | `0x20000000` | `0x20` |

Chaque écriture cible exactement `0x7FEA1A80`, longueur 4, et est validée avant
effet. Aucun paquet audio, sample décodé ou comportement XMA n'est déduit de
ces valeurs. Le replay atteint `max_ticks=5400` sans nouveau trap, puis le
frontier de progression reste l'attente connue `LR=0x822E559C ->
0x822F8848`, clé `0xE000004C`; tous les 23 threads sont bloqués.

Résultat observable : 5263 PRESENT, `frontend=false`, `mission=false`,
`terminal=false`, les IB restent `ef7ab6e4…d2b0` (11 dwords) et
`d121c8d8…358d6` (3029 dwords), et le readback reste non qualifié. Le premier
writer EDRAM non nul et les pixels restent `unknown`.

Artefacts du run frais :

- rapport : `/fastdata/lavaulta/tmp/ac6-cycle1752-proof.Kxn87e/neutral.report.json`, SHA-256 `ec40217ffd824cd7e6f525497b31a05d4c6ca7e14bfdf2746e8de6fc3ce93a44` ;
- trace RTPLY-v4 : `/fastdata/lavaulta/tmp/ac6-cycle1752-proof.Kxn87e/neutral.trace.jsonl`, SHA-256 `793f57e338aa7265f1e4d3c06d0a6d9865fc34da37e0ec9abee1653d6a5c1d6e` ;
- stderr : SHA-256 `646d91c7ec63e9179f9b46672428d77895e286ed3e58ebf0e941df8010e4c223` ;
- binaire codegen-ON : SHA-256 `b31b5f4346b2f50eea40a74ab43b10f2502c1b49ee72dcfafffaf2aaca1d8ccb`.

Le prochain test ciblé est la transition body-side de `0x822F8848` et le
writer EDRAM avant `RB_COPY`, en neutral seulement. START demeure gelé.
