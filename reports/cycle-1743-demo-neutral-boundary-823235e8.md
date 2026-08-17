# Cycle 1743 — frontière neutral `0x823235E8` fermée

La cible indirecte `0x823235E8`, atteinte depuis `LR=0x82323C4C` au tick
2453/thread 1, est une entrée PAL de 24 bytes bornée par les `blr` à
`0x823235E4` et `0x823235FC`. Son hash exact est `5d351518…6b79c`.

Après codegen strict, neutral atteint `max_ticks=2500` avec 2 363 notifications
de présentation et aucun frontend. L'extension à tick 3000 piège ensuite à
tick 2511 sur `0x820DC374 -> 0x820D32B0`. START n'a pas été injecté.

Validations : codegen 12 872 fonctions/150 records configurés, zéro diagnostic;
atlas A/B SHA-256 `d4cbf566…fad22`, couverture `.text` complète; CTest OFF
18/18 et ON 17/17; pytest ciblé 2 passés. L'atlas précédent est sauvegardé
sous `/fastdata/lavaulta/tmp/ac6-demo-static-decomp-atlas-v1.pre1743.json`.

Le prochain batch doit qualifier uniquement `0x820D32B0`, actuellement contenu
dans le chunk `0x820D3230..0x820D3363` (SHA `38ebf8e9…eac63`). Aucun nom
retail, START, pixel ou effet hôte n'est promu.
