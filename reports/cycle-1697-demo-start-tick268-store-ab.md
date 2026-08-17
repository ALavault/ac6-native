# Cycle 1697 — A/B des stores guest autour des objets START au tick 268

## Résultat

Une sonde read-only bornée à `[0x2E3C0000,0x2E3F0000)` capture la fenêtre
guest immédiatement après les trois dispatchs START qualifiés du cycle 1696.
Le run START produit 605 stores au tick 268, tous sur le thread 1; le run
neutral produit 606 stores au même tick. 604 lignes sont strictement
communes. La différence ne constitue pas une transition visuelle : START
ajoute seulement
`0x2E3D3C0C <- 0x00000000` depuis `0x82321E20`, tandis que neutral ajoute deux
écritures de `0x2E3D44F0` (`0` puis `0xFFFFFFFF`) depuis `0x823255E8`.

Les trois arêtes `virtual_dispatch` restent START-only dans le rapport
dynamique, mais aucune écriture d’état non nulle spécifique à START n’est
jointe dans cette plage. Le frontend, les pixels et la légitimité visuelle de
START restent donc explicitement inconnus.

## Identité et protocole

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| basefile | SHA-256 `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA-256 `a9dde7be809f225f63175329e42e52cfa33bbd1f8c689d96039485e5e6349b5e` |
| source de sonde | `src/guest_bridge.cpp`, SHA-256 `e5217db6b098e493612e61da67d5be60026c04211cb91b3f9d79fb46d6283c66` |
| baseline complexité | `config/source-complexity-baseline.json`, SHA-256 `de45ba1333e906a294ce321aa6e63d09d9bd2833889fbce01f6cecbc597fcb97` |
| filtre | `AC6_DEMO_WATCH_TICK_STORES=1`, `AC6_DEMO_WATCH_TICK_RANGE=0x2e3c0000:0x2e3f0000`, ticks 268–269 |
| stores hors pile | oui; la plage est en dehors de la pile guest filtrée `[0x7F000000,0x80000000)` |
| entrée START | bouton `0x10` tenu aux ticks 252–267 |

Chaque route a utilisé un processus et un store neufs, avec le même codegen,
manifestes et configuration. La sonde et le filtre sont désactivés par défaut.

La suite CTest du build atomique passe `17/17`, y compris complexity,
source-audit et status. Le baseline de complexité est désormais présent dans
le chemin référencé par CMake; il ne modifie aucune unité d’exécution.

## Reçus A/B

| route | stores | stderr SHA-256 | RTPLY SHA-256 | rapport SHA-256 | résultat |
|---|---:|---|---|---|---|
| START | 605 | `86fda1bc6c5fc3af9b9978a2263a69611800f38b825c90819c0687baa8bae82b` | `4bf229f4a691d28efb6e979b717120955f7337388190e7109f05bcfa9fb1c273` | `598c6f84a5fbfc44aaeca2c9fb4cacafa1f38c2d2fc81d823971a9c8cb20c621` | 269 ticks, 132 PRESENT |
| neutral | 606 | `eec2429f36cb16c96a89a6959f8964ec1d73141eec0e9306f47b87c373017650` | `919dd6f1fc894526a444d12f0968153cad8acc1cd6baa37ad022a6255a72df7e` | `aacdebaf4412ad38104ccda80eba0163e7e801272d50433af494af5a6b6d9d72` | 269 ticks, 132 PRESENT |

Les deux rapports déclarent `frontend=false`, `mission=false`,
`terminal=false`, `outcome=max_ticks`, le même frontier au tick 269 et les
mêmes manifestes codegen/Ghidra/configuration.

## Différentiel exact

| route | adresse | valeur | PC/LR rapporté | fonction générée | ligne |
|---|---|---|---|---|---:|
| START seulement | `0x2E3D3C0C` | `0x00000000` | `0x820CDC20` | `sub_82321E20` | 24475 |
| neutral seulement | `0x2E3D44F0` | `0x00000000` | `0x823255F0` | `sub_823255E8` | 8352 |
| neutral seulement | `0x2E3D44F0` | `0xFFFFFFFF` | `0x82325644` | `sub_823255E8` | 8404 |

Les 604 lignes communes incluent notamment les écritures d’objets autour de
`0x2E3DDE00..0x2E3DDE68`, `0x2E3DE054..0x2E3DE060` et
`0x2E3DF...`, ainsi que les callsites `0x820CE780`, `0x820D4CF8`,
`0x82323C64` et `0x82323C88`. Elles ne sont pas attribuées à une sémantique
de frontend sans un consumer guest qualifié.

## Classification

- `demo-qualified` : identité PAL, fenêtre/ticks, bornes de mémoire, A/B frais,
  604/605 lignes communes et le différentiel exact ci-dessus.
- `demo-observed` : trois dispatchs START au tick 268 et 605 stores START dans
  la plage filtrée.
- `xenia-generic` : aucun élément.
- `unknown` : rôle des objets, writer/consumer de l’état de frontend, effets
  pixels/audio, mission et terminal. Les trois microcodes et containers ne
  sont pas modifiés ni inventés.

## Garde et prochain checkpoint

Le garde refuse toute promotion visuelle tant qu’un store/consumer guest
causal n’est pas différentiellement joint et que les readbacks ne divergent
pas. La prochaine sonde minimale doit viser les loads/consumers des trois
adresses diff (`0x2E3D3C0C`, `0x2E3D44F0`) au même tick, puis répéter le même
A/B; aucun fallback renderer ou état synthétique n’est autorisé.
