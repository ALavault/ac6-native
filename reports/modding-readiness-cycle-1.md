# Cycle 1 — jalon archive et scène AC6 PAL

Identité : `default.xex`, XEX Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Le jalon est la porte native d'inspection des archives et des scènes :
`DATA.TBL`/PAC, FHM, NDXR/NSXR/MATE, sélection DPL et consommateurs de scène.
Il garde distincts les parseurs, les sorties XenonRecomp générées et les
services Xbox non résolus (`xboxkrnl`, XAM, XMA, Xenos/MMIO).

```sh
ctest --test-dir .build/ace-combat-6 --output-on-failure
```

Résultat : 35/35. Xenia est un oracle disponible mais aucune trace de boot vers
une scène attribuable ne ferme encore le démarrage : statut `blocked-oracle`.
Prochaine action : capture Xenia/XenonTests bornée du boot puis résolution de
la frontière service type `0x98` et de l'identité pointeur entrée-acteur.
