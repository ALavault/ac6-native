# Cycle 1742 — frontière neutral `0x822CD118` fermée

## Résultat

La cible indirecte `0x822CD118`, atteinte depuis `LR=0x8223FF70` au tick
2126/thread 1, est une fonction feuille PAL exacte de huit octets : `lbz` puis
`blr`. Le `blr` précédent à `0x822CD114` et la fonction suivante
`0x822CD120` bornent l'entrée sans nom ou preuve retail.

Après ajout à `confirmed-chunks.toml`, le replay neutral atteint proprement
`max_ticks=2127`. Une extension inchangée à 2500 ticks avance ensuite jusqu'au
nouveau trap fail-closed `0x82323C4C -> 0x823235E8`, tick 2453/thread 1. Elle
observe 2 316 notifications de présentation, mais toujours aucun milestone
frontend, mission ou terminal. START n'a pas été injecté.

## Atlas et validations

- codegen Release : 12 871 fonctions, 149 records configurés, zéro diagnostic
  de frontière ou instruction ;
- atlas A/B byte-identique : SHA-256 `908cb430…0d79e`, 113 frontières
  confirmées nettes et 3 041 220/3 041 220 bytes `.text` classés ;
- CTest codegen-OFF : 18/18 ;
- CTest codegen-ON : 17/17 ;
- pytest schéma/provenance shaders : 2 passés, 64 désélectionnés.

L'atlas 12 870 antérieur est sauvegardé sous
`/fastdata/lavaulta/tmp/ac6-demo-static-decomp-atlas-v1.pre1742.json`.

## Garde et prochain checkpoint

La nouvelle cible `0x823235E8` se trouve dans le chunk statique
`0x823235D0..0x82323647` (SHA-256 `04e96289…e1b0b`), mais ses bornes callable
ne sont pas encore promues. Le prochain batch doit qualifier uniquement cette
entrée sur bytes et contrôle PAL, puis rejouer neutral. Aucun fallback visuel,
START, nom retail ou effet hôte n'est autorisé.
