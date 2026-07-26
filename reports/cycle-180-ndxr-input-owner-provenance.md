# Cycle 180 — provenance de l'entrée `r27+0x40`

Date : 2026-07-18 (Europe/Paris)

## Cible

- target ID : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet Ghidra : `ace-combat-6`
- base : `0x82000000`

Passe headless en lecture seule. Aucun run humain, VNC ou Xenia n'a été
utilisé.

## Contrat d'entrée du caller `0x82233298`

Le prologue conserve :

```text
r3 -> r31   contexte/receiver du caller
r5 -> r27   pointeur d'entrée opaque
f1 -> f28   scalaire flottant entrant
```

Le corps `0x82233298..` emprunte deux branches selon l'état local. Dans la
branche principale, `r27+0x40` est copié mot par mot vers `r1+0x60` aux sites
`0x82233408..0x8223343c` :

```text
arg_r5 + 0x40 -> { word0, word1, word2, word3 }
```

Le buffer `r1+0x60` est ensuite passé au slot virtuel `+0x04` avec `r5=1`,
puis réutilisé comme `r4` de l'appel direct à `sub_822131d0` (`0x82233550`).
La chaîne du cycle 179 est donc maintenant rattachée à un argument entrant
du caller, et non seulement à une valeur locale.

## Branche alternative

La branche vers `0x822337f0` conserve le même `r27`, forme `r30 = r27+0x40`,
appelle `0x82281198`, puis utilise le pointeur `r30` au dispatch du slot
`+0x04` à `0x82233830`. La convergence des deux branches sur le même offset
`+0x40` est confirmée ; leur sémantique reste inconnue.

## Portée de la preuve

- `confirmed` : `r27` est l'argument `r5` du caller `0x82233298`.
- `confirmed` : `r27+0x40` contient au moins quatre mots copiés vers le buffer
  utilisé par le slot `+0x04`.
- `confirmed` : dans la branche principale, ce buffer est réutilisé par
  `sub_822131d0`, puis par le chemin parent `+0x140`.
- `cross-match renforcé` : le même sous-objet `arg_r5+0x40` est utilisé dans
  les deux branches et avec le même receiver partagé.
- `unknown` : type de `arg_r5`, type/unité des quatre mots, rôle de
  `0x82281198`, appelant de `0x82233298` et sémantique gameplay.

Ne pas nommer `arg_r5+0x40` comme position, cellule, avion ou vecteur de jeu.
Le contrat actuel est un sous-objet de quatre mots transmis à plusieurs
méthodes et soumis à des transformations flottantes préservant l'ABI.

## Preuves exécutées

```text
DumpRange.java 0x82233298 0x822333e0
DumpRange.java 0x822333e0 0x82233490
DumpRange.java 0x822334a8 0x82233520
DumpRange.java 0x82233710 0x82233790
DumpRange.java 0x822337f0 0x82233880
```

Aucune action humaine n'est nécessaire pour poursuivre la qualification
statique.
