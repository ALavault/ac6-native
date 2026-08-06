# Cycle 784 — rejet des lecteurs directs de l'axe canonique LY

Date : 2026-08-04

## Résultat

Projet Ghidra canonique `ace-combat-6`, programme `default.xex`, XEX PAL
SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Le scanner borné `scripts/FindPpcDisplacementUses.java` inventorie les accès
mémoire PPC D-form par déplacement sans modifier le projet. Sur `+0x3E`, il
trouve 21 accès, dont une seule lecture halfword directe :

```text
0x8217EAB0  lhz r10,0x3e(r3)  owner=0x8217E9F0
```

Ce candidat est rejeté. La décompilation canonique de `0x8217E9F0` montre une
copie de bloc vers les globaux `0x823FA270..0x823FA2DC`. Son appel qualifié à
`0x821827A8` reçoit `param_2+0x6D8`, et non l'objet device observé à
`0x8290DE3C`. Les autres accès `+0x3E` sont des mots, octets ou stores et ne
lisent pas le champ LY canonique.

`0x8234D110` reste l'unique preuve de dérivation : il copie `device+0x4E` vers
`device+0x3E`, puis charge la source de séparation de signe par `lhzx` après
consultation de la table `0x8201250C`. Par conséquent, l'absence de lecteur
D-form ne signifie pas que LY est inutilisé : le consommateur suivant doit
être recherché parmi les accès indexés ou via la provenance du pointeur
device, pas par un watchpoint limité à l'instruction `lhz disp(rN)`.

## Validation

- exécution headless `-readOnly -noanalysis` sur le projet canonique : succès ;
- scanner compilé et exécuté par Ghidra 12.1.2 : succès ;
- total `+0x3E` : 21 accès ; lecteurs halfword directs : 1 ; candidats device : 0 ;
- build `ac6-native` et test produit ciblé : succès ;
- CTest PAL complet : 64/64, quatre skips retail/headless attendus ;
- aucun changement du projet Ghidra ni du runtime recompilé.

## Prochain test discriminant

Étendre l'analyse statique aux `lhzx` dont l'index provient de la table
`0x8201250C`, puis instrumenter seulement le premier lecteur dont la provenance
de base rejoint l'objet device. Le contrôle dynamique devra conserver les
trois fenêtres pitch positif, nul et retour à zéro, et prouver un store dans
l'enfant `CAce6UnitPlayer` avant qualification du contrat G8.
