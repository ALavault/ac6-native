# Cycle 181 — frontière `.pdata` du caller de l'entrée NDXR

Date : 2026-07-18 (Europe/Paris)

## Cible

- target ID : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet Ghidra : `ace-combat-6`
- base : `0x82000000`

Passe headless et statique, sans intervention humaine.

## Entrée `.pdata`

Le dump de la table à `0x820814b0` donne notamment :

```text
0x820814e8  0x82233298  0x40018405
0x820814f0  0x822338a8  0x40001103
```

En utilisant l'entrée suivante comme borne, le corps associé à
`0x82233298` est donc `0x82233298..0x822338a7`. Il contient à la fois :

- la branche principale `0x82233404..0x82233550` qui copie `r27+0x40`,
  appelle le slot `+0x04`, puis `sub_822131d0` ;
- la branche alternative `0x822337f0..0x82233830` qui réutilise le même
  `r27+0x40` avant un dispatch `+0x04`.

Cette borne `.pdata` évite de traiter les fragments Ghidra courts comme des
fonctions indépendantes.

## Conséquence pour l'appelage

Le balayage `FindDirectCallsTo.java` et `FindPpcBranchesTo.java` ne retrouve
aucun `bl` direct vers `0x82233298`. La présence de l'entrée dans `.pdata`
confirme le corps et sa frontière, mais ne suffit pas à identifier le mécanisme
d'appel. L'hypothèse d'une entrée indirecte/table reste `unknown`; ne pas la
transformer en preuve de vtable ou de callback précis.

## Contrat consolidé

Dans ce corps unique :

```text
r5 -> r27
arg_r5 + 0x40 -> buffer 16 octets
buffer -> slot virtuel +0x04
buffer -> sub_822131d0 (branche principale)
```

La provenance du buffer est donc confirmée au niveau ABI et `.pdata`, tout en
laissant son type et ses unités inconnus.

## Confiance

- `confirmed` : entrée `.pdata`, borne par l'entrée suivante, appartenance des
  deux branches au même corps, copie de `r27+0x40`.
- `unknown` : appelant de `0x82233298`, mécanisme d'entrée, type C++ et rôle
  gameplay de `arg_r5+0x40`.
- `needs-dynamic-evidence` : vtable runtime du receiver partagé, inchangée par
  cette passe.

## Preuves exécutées

```text
DumpDataWords.java 0x820814b0 28
FindDirectCallsTo.java 0x82233298
FindPpcBranchesTo.java 0x82233298
DumpRange.java 0x82233298 0x822333e0
DumpRange.java 0x822333e0 0x82233880
```

Aucune session humaine n'est nécessaire pour la suite statique.
