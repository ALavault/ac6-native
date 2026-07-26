# Cycle 183 — tables statiques contenant l'entrée du caller NDXR

Date : 2026-07-18 (Europe/Paris)

## Cible

- target ID : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet Ghidra : `ace-combat-6`
- base : `0x82000000`

Passe headless statique en lecture seule, sans session humaine.

## Références brutes à `0x82234040`

`FindU32Any.java 0x82234040` trouve exactement :

```text
0x82007a30
0x82009150
0x82081508
```

La dernière occurrence est l'entrée `.pdata` déjà qualifiée. Les deux
premières sont dans des zones de pointeurs de fonctions. Leur voisinage est
structuré de manière répétée :

```text
0x82007a30  0x82234040
0x82007a34  0x82232970
0x82007a38  0x822ddbe8
0x82007a3c  0x82299e68
0x82007a40  0x822663a8
...
0x82007afc  0x82232898
0x82007b00  0x822338a8

0x82009150  0x82234040
0x82009154  0x82232970
0x82009158  0x822ddbe8
0x8200915c  0x82299e68
0x82009160  0x822663a8
...
0x8200921c  0x82232898
0x82009220  0x822338a8
```

La seconde séquence reproduit la première à l'offset `+0x1720` sur la zone
inspectée. Les entrées voisines comprennent notamment les bodies `.pdata`
`0x82232898`, `0x822338a8` et `0x822338f0`, déjà liés à cette famille de
méthodes.

## Interprétation limitée

- `confirmed` : deux tables statiques distinctes contiennent le même pointeur
  `0x82234040` et des séquences voisines de pointeurs de fonctions.
- `cross-match` : la répétition à `+0x1720` et la position identique de
  `0x82234040` renforcent l'hypothèse de tables de dispatch apparentées.
- `unknown` : nature exacte des tables (vtable C++, table de dispatch ou
  autre), adresse-point runtime, constructeur qui instancie l'une des tables et
  objet qui les consomme.
- `needs-dynamic-evidence` : pointeur de vtable effectif du receiver et
  identité métier du payload NDXR.

Il ne faut pas confondre ces deux tables statiques avec la vtable runtime du
receiver partagé `table+0x36084`. Elles ne suffisent pas à promouvoir
`0x82234040` comme méthode d'une classe ou comme opération de gameplay.

## Relation avec le cycle 182

Le direct call `0x822341bc -> 0x82233298` reste confirmé. Cette passe ajoute
seulement la provenance statique de l'entrée `0x82234040` dans deux tables de
pointeurs ; elle ne modifie ni le XEX, ni les projets Ghidra, ni les sorties
générées.

## Preuves exécutées

```text
FindU32Any.java 0x82234040
DumpDataWords.java 0x82007980 112
DumpDataWords.java 0x820090e0 112
DumpRange.java 0x820f8d80 0x820f8f30
```

