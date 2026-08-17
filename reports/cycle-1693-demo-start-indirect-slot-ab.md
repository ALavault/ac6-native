# Cycle 1693 — slot RTTI exact de l’indirect START

## Verdict

Une instrumentation opt-in, strictement read-only, a rejoué neutral et START
depuis des stores/processus neufs avec le même binaire codegen-ON. Neutral
atteint 600 ticks sans cette arête; START maintenu aux ticks 252–267 atteint
à tick 268 `0x82321F34 -> 0x820E7E08`. Au point exact de l’appel, le guest
charge `object=0x2E3D3D14`, `object+0=0x820077AC`, et les deux mots RTTI
`slot[77]=0x820E7E00`, `slot[78]=0x820E7E08`. La sélection du slot 78 est
donc unique parmi les candidats statiques observés.

Cette preuve permet de qualifier la jointure objet/vtable/slot pour ce site,
mais pas encore la sémantique du stub ni une transition frontend. Le runtime
reste fail-closed sur la cible non mappée comme fonction.

## Identité et A/B

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| basefile PAL | `.build/ac6-demo-atomic-runtime-1/codegen/xex-basefile.bin`, SHA `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire instrumenté | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA `aa24028d533aeef28eab88e58cbfd2f8849a8a7a5a38395713588d897e2d1581` |
| hook | `AC6_DEMO_WATCH_INDIRECT_OBJECT=1`, désactivé par défaut |
| instrumentation | lit uniquement guest memory; aucun store guest |

| route | résultat | RTPLY SHA | rapport SHA | stderr SHA |
|---|---|---|---|---|
| neutral, 600 ticks | `max_ticks`, 463 PRESENT, aucun frontend/mission/terminal | `b724112495de5af96b395f93ba5d1dacd8a522bfae2aa1fcc69890b3c9aac9fb` | `dbe6484af6c1446db0e2e5017b34441d909cc07e81922959d8ad9d9719b16484` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| START tenu ticks 252–267 | trap tick 268, 131 PRESENT, aucun frontend/mission/terminal | `db6d8a5d31f80022868e30cd4f3335339dddada4c7e2f3bbf8d71756cb76536b` | `40aab8bf769cd512bcb1f4f0f37c2ca50445e0719f6210d353272d82b1a34586` | `7fb995872bc97c5e3ce5971691aadcfc6233d9a31e7c9f52a7b7d1d649dfa6ba` |

Les RTPLY et rapports neutral/START sont identiques à ceux des probes
précédents malgré le hook; le seul changement stderr START est la ligne de
capture suivie du trap `unqualified guest indirect call`.

## Capture dynamique exacte

La ligne read-only est :

```
AC6_INDIRECT_OBJECT lr=0x82321F34 target=0x820E7E08 tick=268 thread=1 object=0x2E3D3D14 object_mapped=1 vtable=0x820077AC slot77_mapped=1 slot77=0x820E7E00 slot78_mapped=1 slot78=0x820E7E08 r10=0x820077AC
```

La cible apparaît une fois (`count=1`) dans le rapport frontier, avec
`virtual_dispatch=null` avant cette instrumentation. La lecture prouve
maintenant le triplet `(object, vtable, slot 78)`; elle ne donne aucun nom de
classe ni sémantique métier.

## Jointure PAL statique

Le basefile PAL et l’atlas statique donnent :

- chunk non possédé `0x820E7E00..0x820E7E0F`, bytes
  `38a000014bffa17438a000004bffa16c`, SHA
  `988a4167846e4f600a58f19d51a7e694a854985677cad48f0e5d73f1efda9180`;
- vtable `0x820077AC`, slots 77/78 = `0x820E7E00`/`0x820E7E08`;
- vtable homologue `0x820065A4`, mêmes slots 77/78;
- la frontière suivante `.pdata` reste `0x820E7E10..0x820E80D3`, donc le
  chunk n’est pas scindé en fonction Ghidra;
- le site statique appartient à `0x82321E20..0x82321F6B`, avec les hashes
  bytes/pseudocode déjà scellés au cycle 1692.

La correspondance dynamique avec `0x820077AC` est désormais directe par
`object+0`; aucune preuve retail, Xenia ou ReXGlue n’est utilisée.

## Classification et garde

- `demo-qualified` : A/B frais, identité PAL, lecture `object+0`, vtable
  `0x820077AC`, sélection unique slot 78, bytes du chunk et cible exacte.
- `demo-observed` : route START maintenue et divergence au tick 268.
- `xenia-generic` : aucun élément.
- `unknown` : sémantique des deux stubs, ABI métier, état frontend, pixels,
  audio, mission et terminal.

La prochaine intégration autorisée est un stub de branche borné et vérifié par
les 8 bytes de l’entrée `0x820E7E08`, qui positionne uniquement `r5=0` puis
redirige vers la cible littérale `0x820E1F78`; il doit rester distinct d’une
nouvelle frontière Ghidra et trapper si les bytes ou la cible changent. Avant
promotion, refaire un A/B frais après ce shim et conserver l’absence de
frontend comme résultat tant qu’aucun état guest causal n’est prouvé.

