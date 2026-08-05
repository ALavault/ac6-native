# Mission 01 — campagne render selector boundary

Date: 2026-08-05 (Europe/Paris)

## Verdict court

Le chemin prioritaire « gameplay reste dans une vue sans bit maître 3D » n'est
pas observé dans le run instrumenté. Les frames de gameplay C5 et C6 passent
par `full_3d`, avec `manager+0x29C != nullptr`. Le forçage bridge causal n'a
donc pas été exécuté : aucune valeur fautive n'a été qualifiée à forcer.

Le sélecteur n'est pas promu comme cause du monde noir. La prochaine frontière
est le graphe `draw -> RT -> resolve -> composite`, après la qualification
séparée de l'enregistrement/consommation des ressources entry 119.

## Provenance et lane

* Projet Ghidra canonique : `ghidra-projects/ace-combat-6`.
* Target : PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
* `DATA.TBL`, SHA-256
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5`.
* Source bridge externe : commit `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`,
  worktree dirty, diff déterministe actuel
  `f5d8f3d54d0716a057695329501ae44a341125e3fed4c76084961b34a02e2741`.
* Binaire exécuté : SHA-256
  `9e0ef14cc07a6c1c1b584b73b30d989683a791790f15f75fb58f71cfe2e8bdd4`.
* Lane runtime : `bridge` (`save-dialog-synthesis`, cvars forcés, fallback
  allocator). Les résultats ne sont pas une preuve `stock`.
* Backend visible : Vulkan RexGlue/Xenia. Aucun renderer natif ou replay n'a
  été sélectionné.
* Configuration effective : performance off, debug, unlock FPS off, capture
  render/frontier on, signature diagnostics on, D3D trace off, toutes les
  échelles à 1, direct host resolve off, invalid fetch constants off. Le
  marqueur `[ac6-baseline-config]` du run confirme ces valeurs.

## Fingerprints loaded-image

La règle `loaded RVA - raw offset = 0x3600` a été respectée. Les hooks ont été
posés seulement après vérification du SHA XEX et des octets :

| frontière | octets qualifiés | résultat |
| --- | --- | --- |
| `0x821A16B8` | `7d8802a6481e18053981ff68481e2d85` | entrée qualifiée |
| `0x821A1704` | `7e4ba02e` | `lwzx r18,r11,r20` |
| `0x821A1884` | `564b06f62b0b0000419a14c08174029c` | test du bit maître |
| `0x821A0DFC` | `817c01e0556b05ee2b0b0000419a01fc...` | préparation `+0x29C` |
| `0x8226DF00` | `388b8184816a00087d6903a64e800421...` | marqueur UpHud |
| `0x8226DF1C` | `807c029c` | charge `manager+0x29C` |

Les sept écritures candidates sont également des `stw ...,0x260(...)` avec les
octets exacts consignés dans
`analysis/render/campaign_view_writes.jsonl`. Une seule (`0x822E5310`) a une
fonction parente définie par le projet canonique (`0x822E5280`); les six autres
restent sans frontière de fonction définie. Elles ne reçoivent donc aucun rôle
runtime déduit de `ctx.lr`.

## Route observée

Le run `cycle-1026-campaign-selector` a produit 1 067 observations de route et
11 écritures du champ `manager+0x260`. Les valeurs représentatives sont dans
`analysis/render/campaign_render_routes.jsonl`.

* C3, première observation après la cinématique : vue effective 1, masque nul,
  `exit_mask_zero`.
* C4, premier tick caméra observé : vue 8, masque `0x4000001F`,
  `full_3d`.
* C5, frame stable HUD partiel + monde noir : vue 2, masque brut
  `0x4000007F`, masque effectif `0x4000003F`, bit `0x10` présent,
  `full_3d`.
* C6, après plus de 60 ticks : mêmes valeurs et `full_3d`.
* C7, après la fenêtre d'entrée de vol : vue 3, masque `0x4000001F`,
  `full_3d`.

Le masque effectif observé en C5/C6 contient donc le bit 3D. L'écran reste noir
alors que la branche qui saute `ReMapPre`/`ReObj`/`ReMap` n'est pas prise.

## Écritures de vue

Les changements runtime sont réels et la transition n'est pas absente :

```text
1 -> 0 -> 1 -> 2 -> 1 -> 8 -> 1 -> 2 -> 1 -> 0 -> 3
```

Les valeurs et frames sont conservées dans le JSONL. Le watcher du corpus
généré ne fournit pas de guest PC exact (`pc=0`). Le champ `lr` est enregistré
uniquement comme contexte; il n'est pas présenté comme l'adresse de
l'instruction d'écriture. La question « quelle instruction exacte installe la
vue gameplay ? » reste ouverte tant qu'une instrumentation PC littérale ou une
frontière parente réconciliée n'est pas disponible.

## Contexte `manager+0x29C`

Le fingerprint statique de la préparation montre le test de `manager+0x1E0`
contre `0x100` et l'écriture de `manager+0x29C`. En runtime gameplay,
`manager+0x1E0 = 0xFFFFFFFF` (bit `0x100` présent) et
`manager+0x29C = 0xB0E8CE20` aux frames C5/C6. Le contexte n'est donc pas nul.

Le wrapper de la fonction parente candidate n'a toutefois pas produit de
record direct; l'exécution exacte de l'installation, et une remise à zéro
ultérieure, restent `unknown`. Cela ne change pas le résultat de la branche
observée : le test runtime `+0x29C == nullptr` est faux.

## Checkpoints image

Les captures sont conservées hors dépôt dans le run. La cinématique tardive
`step-80-post-cinematic-a-15s.png` est texturée (avion visible), tandis que
`step-83-flight-hud-baseline.png` montre les primitives HUD sur le monde noir.
Les hashes et statistiques sont dans le JSONL; les captures ne sont pas
commitées.

## Statut des affirmations

| affirmation | statut |
| --- | --- |
| A — gameplay utilise une vue sans bit maître 3D | `rejected` pour C5/C6 |
| B — contexte `+0x29C` non initialisé | `rejected` pour C5/C6; installation exacte `open` |
| C — transition cinématique → gameplay absente | `rejected` au niveau comportemental; PC exact de l'écriture `open` |
| H — tous les gates sont corrects, défaut en aval | `strongly_supported`, pas encore `qualified` avant register/consumer entry 119 |

## Décision

Aucun run `force_campaign_render_route` n'est justifié par cette observation;
il n'aurait pas une variable fautive à discriminer. Aucun shader, texture,
resolve, MRT, input ou HSM n'a été modifié dans ce slice.
