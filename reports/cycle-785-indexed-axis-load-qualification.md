# Cycle 785 — qualification de l'accès indexé de la table d'axes

Date : 2026-08-04

## Résultat

Le scanner `scripts/FindIndexedAxisLoads.java` reconnaît les matérialisations
non adjacentes de `0x8201250C`, puis cherche les `lhzx` dont l'une des deux
entrées est `r3` (les opérandes PPC sont commutatives pour l'adresse effective).
Le projet Ghidra canonique `ace-combat-6` produit un seul candidat :

```text
table=0x8234D124  load=0x8234D150  index_reg=r10  dest=r10
owner=0x8234D110 FUN_8234D110
```

La séquence qualifiée est :

```text
0x8234D144  lwz   r10,-4(r11)
0x8234D148  addi  r10,r10,0x14
0x8234D14C  rlwinm r10,r10,1,0,30
0x8234D150  lhzx  r10,r10,r3
```

Elle correspond à la boucle de `0x8234D110` qui lit la table de sélection,
calcule un offset de champ et lit l'état canonique dans l'objet pointé par
`r3`. Le candidat est donc retenu comme consommateur indexé de la dérivation
des axes, mais `r3` n'est pas encore qualifié comme le device live
`0x8290DE3C` au moment de cette lecture.

## Validation

- Ghidra headless 12.1.2, projet canonique, `-readOnly -noanalysis` : succès ;
- candidats trouvés : 1 ;
- aucune modification du binaire, du projet Ghidra ou du runtime recompilé ;
- build/test produit et CTest PAL 64/64 validés au checkpoint précédent.

## Prochain test discriminant

Instrumenter l'entrée `0x8234D110` ou son appel immédiat en capturant `r3`, la
valeur d'index et le résultat `lhzx` durant les fenêtres pitch positif, nul et
retour à zéro. Ne retenir le contrat que si `r3` rejoint l'objet device
canonique et si la valeur lue est consommée par le chemin enfant joueur.
