# Cycle 1692 — nouveau frontier START, appel indirect `0x820E7E08`

## Verdict

Un probe START codegen-ON, avec process et store neufs, maintient START aux
ticks 252–267 et atteint un nouveau frontier au tick 268. Le runtime trappe
sur l’appel indirect guest `0x820E7E08` (thread 1, LR `0x82321F34`). Le run
ne produit aucun jalon frontend, mission ou terminal : il s’agit d’une
borne de décompilation, pas d’une validation de la transition START.

## Identité et artefacts

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| basefile PAL | `.build/ac6-demo-codegen-atomic-1/xex-basefile.bin` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp` |
| binaire SHA-256 | `ed44dba583e2aa78d081ca52796ddc0e52b7b775015db41ca1198814c95b2ecc` |
| rapport runtime | `/fastdata/lavaulta/tmp/ac6-start-hold600.report.json` |
| rapport SHA-256 | `40aab8bf769cd512bcb1f4f0f37c2ca50445e0719f6210d353272d82b1a34586` |
| RTPLY | `/fastdata/lavaulta/tmp/ac6-start-hold600.rtply` |
| RTPLY SHA-256 | `db6d8a5d31f80022868e30cd4f3335339dddada4c7e2f3bbf8d71756cb76536b` |
| stderr | `/fastdata/lavaulta/tmp/ac6-start-hold600.stderr` |
| stderr SHA-256 | `41b7a8d793cd512bcb1f4f0f37c2ca50445e0719f6210d353272d82b1a34586b` |
| résultat | trap à 268 ticks, 131 PRESENT, frontend/mission/terminal faux |

Le rapport runtime est `ac6-demo-frontier-report/v1`; sa frontière est
`unqualified guest indirect call`, tick 268, LR `0x82321F34`, cible
`0x820E7E08`.

## Observation dynamique

| champ | valeur observée |
|---|---|
| thread | `1` |
| tick | `268` |
| LR rapporté | `0x82321F34` |
| cible | `0x820E7E08` |
| compteur de cette arête | `1` |
| `r1` | `0x7F040768` |
| `r3` | `0x2E3D3D14` |
| `r4` | `0x2E3CFA08` |
| `r10` | `0x820077AC` |
| `r11` | `0x820E7E08` |
| `r12` | `0x82321F1C` |
| `r13` | `0x7F000000` |
| `r26` | `1` |
| `r28` | `0x2E3CF0D4` |
| `r30` | `0xFFFFFFFF` |
| `r31` | `0x2E3D3AD4` |

Le champ `virtual_dispatch` du rapport est `null`: le site LR
`0x82321F34` ne fait pas encore partie des sites de dispatch virtuel
qualifiés. La valeur `r10=0x820077AC` est une coïncidence d’adresse observée
avec une vtable statique; elle ne suffit pas à prouver le chargement de la
vtable ni le slot effectivement sélectionné.

## Jointure statique PAL

Le manifeste Ghidra démo décrit le chunk non possédé
`0x820E7E00..0x820E7E0F`, hash bytes
`988a4167846e4f600a58f19d51a7e694a854985677cad48f0e5d73f1efda9180`.
Les bytes du basefile sont exactement :

```
38 a0 00 01 4b ff a1 74 38 a0 00 00 4b ff a1 6c
```

`0x820E7E10` commence ensuite une fonction `.pdata` distincte, hash bytes
`c5de9580b1a539467c2a31739c6fca52f15bd002ff4c9c2c4ea751eee4136e93`,
avec pseudocode hash `ef7acd7399fd7309b2303df5b44b91c89ba076799fd7f09ee637ad277cc58fbb`.
Le généré littéral recoupe les deux stubs du chunk (`li r5,1` et `li r5,0`)
mais ne transforme pas le chunk en deux frontières Ghidra.

L’atlas RTTI démo contient les mêmes deux cibles dans les slots 77 et 78 des
vtable `0x820065A4` et `0x820077AC`. Le slot 78 pointe vers `0x820E7E08`;
les descripteurs ont respectivement les hashes de nom
`7ed19cde0c4cd722d4823fe7c9998748fa97bb997b88b7b26f0f6c237c0502a1` et
`58528444d07c4912c91a413d12fd8e33c01b908efaa5b328bf49e0bfb058a0b4`.
La fonction contenant le site statique est `0x82321E20`, range
`0x82321E20..0x82321F6B`, bytes hash
`1c0454f49aad8ce00009905808b90cf6278a464e1eb6671e1b7397ef605419c9`;
son pseudocode hash est `d83cfa1c85bfe7e1dd911f4f9b784448e36d68fb2ac4a548b5a16b563d28f51f`.

## Classification

- `demo-qualified` : identité XEX/basefile, trap dynamique exact, bytes du
  chunk, frontière `.pdata` suivante et cibles RTTI des deux vtables.
- `demo-observed` : exécution maintenue START, `r10=0x820077AC` et passage
  par la région statique du site `0x82321E20`.
- `xenia-generic` : aucun élément utilisé.
- `unknown` : ABI de `0x820E7E08`, slot dynamique effectivement chargé,
  objet/vtable au point exact de `bctrl`, sémantique de l’état, frontend,
  pixels, audio, mission et terminal.

## Garde et prochain checkpoint

Ne pas implémenter ni renommer `0x820E7E08`, ne pas scinder le chunk
`0x820E7E00..0x820E7E0F`, et ne pas promouvoir START. Le prochain test doit
capturer, pour le même LR et un run neutral/START frais, la lecture de
`object+0`, l’adresse de vtable et le mot de slot juste avant le `bctrl`;
la table de dispatch ne pourra être ajoutée qu’après égalité bytes/basefile
et sélection unique du slot. Toute cible ou valeur divergente reste
fail-closed.

