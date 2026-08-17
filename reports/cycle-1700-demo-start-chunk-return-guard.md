# Cycle 1700 — retour du thunk START et garde de branche précoce

## Résultat

La sonde `AC6_DEMO_WATCH_CHUNK_RETURN=1` imprime le contexte après l’appel
qualifié `0x820E7E08 -> 0x820E1F78`. START atteint exactement un retour au
tick 268; neutral n’atteint aucun retour de ce thunk. Le retour START conserve
`r5=0`, mais ne publie aucun état frontend : `r3` pointe une zone de pile et
les registres callee-saved sont restaurés (`r21=0`, `r27=0`).

Les loads combinés du cycle 1699 fournissent la jointure statique/dynamique
suivante, sans modifier le code généré :

1. `r26 = object + 336 = 0x2E3D3E64`;
2. le load `4(r26)` lit `0x2E3D3E68 = 0x2E3D4050`;
3. le load `0(r21)` lit `0x2E3D4050 = 0x2E3D4050`;
4. le bytes PAL du thunk comparent ensuite `r27` et `r21` à `0x820E1FE0`;
5. l’inventaire d’arêtes ne contient aucun appel indirect aux callsites
   `0x820E2018`/`0x820E205C` dans cette exécution.

La branche égale vers la sortie précoce est donc une qualification de contrôle
et non une preuve de sémantique métier. Aucun readback, frontend, mission ou
terminal ne devient qualifié.

## Identité et A/B

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| basefile | SHA-256 `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA-256 `a6f605156adce9f31209c8759db96d1ffa3c8e8fcac23b1a0c9cf8dcc45c2359` |
| source principal | `src/guest_bridge.cpp`, SHA-256 `fc2b765e5ccea74b2d65932880b3c4e876b690c4c6dc82bff579a6eeca9e7159` |
| hook | `AC6_DEMO_WATCH_CHUNK_RETURN=1`, `AC6_DEMO_WATCH_INDIRECT_OBJECT=1`, désactivé par défaut |
| fenêtre | ticks 268–269, START `0x10` tenu aux ticks 252–267 |

| route | retours thunk | RTPLY SHA-256 | rapport SHA-256 | stderr SHA-256 | PRESENT |
|---|---:|---|---|---|---:|
| START | 1 | `4bf229f4a691d28efb6e979b717120955f7337388190e7109f05bcfa9fb1c273` | `598c6f84a5fbfc44aaeca2c9fb4cacafa1f38c2d2fc81d823971a9c8cb20c621` | `50df21f818b941b1e028d557b53a58efe38f4cdbefdc9952334e85b29f1a4fc3` | 132 |
| neutral | 0 | `919dd6f1fc894526a444d12f0968153cad8acc1cd6baa37ad022a6255a72df7e` | `aacdebaf4412ad38104ccda80eba0163e7e801272d50433af494af5a6b6d9d72` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | 132 |

Les deux rapports sont `max_ticks` à 269 ticks, avec
`frontend=false`, `mission=false`, `terminal=false`.

## Retour START exact

```text
AC6_CHUNK_RETURN target=0x820E1F78 tick=268 thread=1
r1=0x7F040768 r3=0x7F0406F8 r4=0x00000001 r5=0x00000000
r21=0x00000000 r26=0x00000001 r27=0x00000000 r28=0x2E3CF0D4
r29=0x00000000 r30=0xFFFFFFFF r31=0x2E3D3AD4 lr=0x82321F34
```

Le texte ci-dessus est un reçu de sonde, pas une attribution de classe. La
sortie exacte et les bytes du thunk restent ceux de l’atlas PAL.

## Classification et garde

- `demo-qualified` : retour START-only, `r5=0`, loads exacts aux deux adresses,
  bytes/contrôle statique et absence d’arête indirecte aux deux `bctrl`.
- `demo-observed` : branche précoce et registres de retour hôte/guest.
- `xenia-generic` : aucun élément.
- `unknown` : rôle du thunk, consumer après retour, état visuel, pixels,
  audio, mission et terminal.

La garde refuse toute promotion START : la branche précoce ne produit pas de
state guest qualifié et aucun readback différent n’est observé. Le prochain
test doit suivre le caller après `0x82321F34` ou les champs objet réellement
consommés, avec A/B frais et arrêt fail-closed sur tout appel indirect inconnu.

