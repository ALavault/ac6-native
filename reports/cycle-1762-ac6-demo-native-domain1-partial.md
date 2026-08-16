# AC6 `ac6-demo-native` domaine 1 — checkpoint cycle 1762

Verdict : **PARTIAL/NO-GO**, `supported=false`.

Ce checkpoint qualifie la base autonome `ac6-demo-native` en frontière
`import-only` : CLI `import`/`verify`, store séparé, VFS read-only `game:/`,
identité PAL démo et corpus exact de neuf fichiers. Il ne qualifie aucune
scène, simulation, PPM, Vulkan, FSM, mission, frontend, XAM/XMA runtime ou
parité native. L’identité exclusive est `ac6-demo-xbox360-pal`, Xbox 360
Xenon/PAL-demo, `Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
L’XEX retail `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
les sorties générées et les projets Ghidra historiques sont hors identité et
hors preuve.

La base vient des commits poussés
`1f3f720ae400ef4e18a04aa7b7bfbe3112682088` (2026-08-17T00:54:47+02:00,
identité/store/VFS) et
`6de7e190c1eee6d8e9a8f0be5828a80ca9ef18e9` (2026-08-17T01:23:49+02:00,
durcissement de publication), tous deux atteignables depuis
`origin/static-scenario-schema-microexec`.

## Validations

Les neuf fichiers de `demo-game-file/extracted/stfs-root` correspondent aux
tailles et SHA-256 du profil scellé, soit **322 371 032 octets**. Le profil
porte `supported=false`, `target_id=ac6-demo-xbox360-pal` et le namespace
`game:/`.

```text
cmake --build reconstruction/ac6-demo-native/build -j16       PASS
SDL_AUDIODRIVER=dummy ctest ...                                3/3 PASS
cmake --install ... --prefix $PWD; test ! -e bin/bin            PASS
bin/ac6-demo-native verify --store <store isolé>                PASS
production_surface / isolation                                 PASS
```

Le CTest couvre `ac6-demo-native-content`, `ac6-demo-native-vfs` et
`ac6-demo-native-production-surface`. L’installation contient
`bin/ac6-demo-native` et le profil sous
`share/ac6-demo-native/config/`; aucun `bin/bin` n’est créé. La surface de
production ne laisse pas fuir l’API fixture, refuse le profil CLI injecté et
ne lie pas `ac6_product_core`, `ac6_demo_runtime`, la recompilation démo ou un
runtime retail.

## Bloquants de publication

- **P1 — TOCTOU publish** :
  `reconstruction/ac6-demo-native/src/content_store.cpp:494-560,570-599`.
  La séquence staging/génération, validation et publication du pointeur n’est
  pas encore fermée contre le swap concurrent requis ; la publication
  atomique n’est donc pas un contrat de crash/concurrence qualifié.
- **P1 — cleanup étranger** :
  `content_store.cpp:506-520`, `posix_fd.cpp:419-452` et
  `content_store.cpp:367-374`. Le nettoyage d’échec n’est pas qualifié contre
  le remplacement/ownership d’une entrée par un autre processus.
- **P2 — parent-dir fsync** : `posix_fd.cpp:128-175`. La création par
  `mkdirat` n’a pas de `fsync` parent qualifié à la frontière de création ; la
  durabilité après panne reste ouverte.

Les tests manquants sont explicitement : multi-processus réel, swap concurrent
de publication, injection d’échec `rename`/`fsync` avec rollback, et erreurs
d’E/S forcées sur chaque frontière d’écriture/synchronisation/nettoyage. Les
3/3 CTest ne ferment pas ces risques.

Le **domaine 2 est NO-GO** malgré son plan read-only : aucun runtime, frontend
ou mission n’est câblé ou promu. La frontière XAM du cycle 1761 reste séparée
et n’est pas déclarée fermée ici. Aucun claim retail, Xenia, C++ généré ou
Ghidra historique n’est utilisé.

## Handoff / prochaine action

Une passe corrective supplémentaire requiert une **autorisation explicite** ou
une réévaluation par root. Les deux tentatives Luna autorisées sont épuisées
uniquement comme note de handoff/next action ; ce n’est pas une preuve
technique et cela ne justifie aucune promotion de support.
