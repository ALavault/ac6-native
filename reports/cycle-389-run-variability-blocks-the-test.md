# Cycle 389 — la variabilité entre exécutions bloque le test

## 1. Trois tentatives, trois échecs distincts

| tentative | séquence | `skip fired` | deltas image | état atteint |
|---|---|---:|---|---|
| 387 | 22 s, A x3, Start, A, A | 0 | 0 partout | sauvegarde (statique) |
| 388 | idem, valeur décimale | 0 | ~19 000 | écran animé |
| 389 | 33 s, Start, A, A (séquence éprouvée) | 0 | ~20 000 | écran animé |

La séquence dite « éprouvée » — celle qui avait mené à l'écran de sauvegarde aux
cycles 382 et 383 — **n'y mène plus**. Le parcours jusqu'à cet écran n'est pas
reproductible d'une exécution à l'autre.

## 2. Deux inconnues, toujours superposées

`skip fired = 0` dans les trois cas admet encore deux causes :

1. l'exécution n'atteint pas l'état où `0x03514000` est lié — établi pour 388
   et 389 par les deltas non nuls, qui trahissent un écran animé ;
2. le cvar `ac6_skip_texture_base` **n'est pas lu du tout** — jamais vérifié.

La tentative 387 est le cas gênant : elle **était** sur l'écran de sauvegarde
(deltas nuls), `0x03514000` y est lié (cycle 382), et l'omission n'a pourtant pas
tiré. Cela pointe vers (2), sans le prouver : la valeur y était passée en
hexadécimal, que l'analyseur de cvar rejette peut-être.

## 3. Le contrôle qui manque encore

Journaliser, au démarrage, la valeur effectivement lue :

```c
REXGPU_INFO("[ac6-skip] ac6_skip_texture_base = {:#010x}",
            uint32_t(REXCVAR_GET(ac6_skip_texture_base)));
```

Une ligne. Elle sépare définitivement « cvar non lu » de « état non atteint », et
elle aurait dû accompagner la sonde dès le cycle 386 — au même titre que le
compteur d'omissions ajouté au cycle 388.

Deux contrôles étaient nécessaires, pas un : **la sonde a tiré** *et* **le
paramètre est arrivé**. J'en avais posé un seul.

## 4. État

Le test qui trancherait l'attribution du cycle 385 reste **non exécuté**. L'outil
est correct, un contrôle sur deux est en place, et l'accès fiable à l'écran de
sauvegarde n'est pas acquis.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.
