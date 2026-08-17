# Cycle 1683 — reachability neutral/START jusqu’au tick 1000

## Résultat

Une comparaison process-fresh correcte prolonge le corridor PAL démo jusqu'au
tick 1000. Le run neutral n'injecte aucune entrée; le run START injecte
`buttons=0x10` au tick 252. Les deux routes atteignent exactement les mêmes
2 457 fonctions, 1 078 arêtes indirectes et 581 imports. La fonction ajoutée
entre le checkpoint 800 et 1000 est `0x820D2D28` (2 appels au tick 986 dans
chaque route) et est commune aux deux routes; aucune fonction START nouvelle
n'apparaît. Son record statique a le hash de bytes
`a7f4a346…47c842b` et reste de rôle `unknown`.

Le seul écart de contenu d'atlas reste la cadence des deux wrappers de section
critique `0x822E1DF0/0x822E1DF8`, `-168` sous START. Le caller
`0x820FF8D8` garde 748 appels dans les deux routes. Les IB, le nombre de
PRESENT, le `XE_SWAP`, la file de rendu et les états de milestone sont
identiques. Ce résultat ne qualifie toujours pas une transition frontend ou
un consumer guest-owned.

## Identité, commandes et artefacts

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon PPC big-endian / Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| runtime | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA `ed44dba583e2aa78d081ca52796ddc0e52b7b775015db41ca1198814c95b2ecc` |
| backend | `headless`, codegen ON |
| neutral | clone `neutral-store-correct`, sans `--input-at` |
| START | clone `start-store`, `--input-at 252,16,0,0,0,0,0,0,1` |
| limite | `--until terminal --max-ticks 1000` |
| racine temporaire | `/fastdata/lavaulta/tmp/ac6-demo-reachability-5000.zCSUzX/` |

| artefact | neutral | START |
|---|---|---|
| atlas | `5166afcc9c6fae78f34cf1037d73ae881003579ce875474d1b0c0e13ca33fcf6` | `8ef27d4d4ffa87ffdb6fdae5d7aaaa2507126b2adf01876d426a1f71e7cdc36f` |
| rapport | `4023fc0fae3b75c1bcd3386e303346dae7d2b9920958f1da429d15bc9c481d5c` | `6031c8ea0c26e591ce246e51297e7de6ab9cba7f4a90c4f71d772d5bd8c5ed3e` |
| RTPLY-v4 | `790c6d9dd3931f8f4c2b5224afe3480d01f1460d1bc6a1f324b65d06bbe718a3` | `993cbe344b60d6fd675d66c8719f74488138c9be802d96ce06957d23efa06657` |
| XAM movie | `9f1b80ed55109fccc470a152ab849f1139e942d4305b3ac58f986183fd796e03` | `d6de5cf324eaa9777db6cdbead8a76c0079b281e308eaa2aa04dce0f5804738c` |

Les clones de store restent sous `TMPDIR`; aucun artefact temporaire n'est
copié dans le produit.

## Comparaison déterministe

| corpus | neutral | START | comparaison |
|---|---:|---:|---|
| fonctions | 2 457 | 2 457 | ensembles identiques |
| arêtes indirectes | 1 078 | 1 078 | enregistrements identiques |
| imports | 581 | 581 | enregistrements identiques |
| `0x820FF8D8` | 748 | 748 | identique |
| nouvelle `0x820D2D28` | 2 (tick 986) | 2 (tick 986) | commune |
| `0x822E1DF0` | 785 782 | 785 614 | −168 |
| `0x822E1DF8` | 785 782 | 785 614 | −168 |
| `xboxkrnl` ordinal 293, LR `0x820FF91C` | inchangé hors wrapper | inchangé hors wrapper | cadence −168 jointe |
| `xboxkrnl` ordinal 304, LR `0x820FF93C` | inchangé hors wrapper | inchangé hors wrapper | cadence −168 jointe |

Les deux wrappers ont `first_tick=252` et `last_tick=999` dans les deux
atlas. La comparaison récursive des rapports ne trouve, outre le hash de
trace, que les deux compteurs d'arêtes correspondant à ces wrappers.

## Rendu observé dans le même run

Les deux rapports donnent 863 notifications de présentation et 863 appels
`VdSwap`, au tick 999, avec :

- ring PM4 base `0x126CA000`, capacité 131 072 dwords, deux soumissions;
- IB intermédiaire `0x127CA0C0/11`, SHA `ef7ab6e4…d2b0`;
- IB principal `0x1274A000/3029`, SHA `d121c8d8…358d6`;
- frontbuffer `0x1374A000`, format `6`, 1280×720, fetch identique;
- `frontend=false`, `mission=false`, `terminal=false`;
- file render queue : producteur 1 495, consommateur 0, même état A/B.

Ces observations sont une répétition structurale du flux PM4; elles ne
constituent pas un readback guest-owned ni une screencap.

## Qualification

- `demo-qualified` : identité PAL, A/B borné à 1000 ticks, ensembles de
  fonctions/arêtes/imports, IB, swap et paramètres de présentation identiques.
- `demo-observed` : nouveau palier commun à 2 457 fonctions et écart `-168`
  persistant des wrappers critiques après START.
- `xenia-generic` : aucun élément utilisé.
- `unknown` : transition causale frontend, consumer guest-owned, pixels,
  audio, mission, résultat et screencap.

Une première tentative neutral qui portait par erreur le même `--input-at` que
START a été rejetée; elle n'est incluse dans aucun hash ni conclusion.

## Prochain checkpoint

Poursuivre au-delà de tick 1000 avec un hook guest ciblé sur la consommation de
la file ou de l'état XAM, en gardant les routes sans écriture synthétique. Une
variation de cadence des critical sections ne suffit pas à promouvoir START ni
à activer un readback de production.
