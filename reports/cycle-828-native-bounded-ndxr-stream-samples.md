# Cycle 828 — native bounded NDXR stream samples

Date: 2026-08-04

## Resultat

Le produit natif expose maintenant `DecodedGeometry` pour les buffers NDXR
verifies par `QualifiedBufferDatabase`.

Le decodeur reste volontairement borne :

- maximum 4 vertices echantillonnes ;
- maximum 8 indices echantillonnes ;
- positions `x/y/z` lues comme trois floats little-endian au debut du vertex
  stride ;
- indices lus en little-endian 16-bit ou 32-bit selon la section `IDX`.

## Garde fail-closed

`NativeGeometryDatabase::load_verified` refuse maintenant :

- un buffer non verifie ;
- un header/sections NDXR incoherents ;
- un payload `DATA` trop court ;
- un `vertex_stride < 12` ;
- une lecture partielle de sample ;
- un index echantillonne superieur ou egal a `vertex_count`.

## Validation

Commandes executees depuis la racine du workspace :

```text
cmake --build reconstruction/ace-combat-6/build -j2
ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
```

Resultat : `1/1` test passe.

Audits :

```text
strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'
ldd reconstruction/ace-combat-6/build/ac6-native
rg -n 'mission_id_ == 1|mission_id == 1|assets\.has\(9\)|assets\.has\(119\)' reconstruction/ace-combat-6/include reconstruction/ace-combat-6/src
```

Resultat : aucun marqueur Xbox/oracle/PPC dans le binaire, dependances Linux
standard uniquement, aucun branchement produit Mission 01 ou asset 9/119
hardcode dans `include`/`src`.

## Limites

Ce cycle ne prouve pas encore le rendu mesh retail Mission 01. Il convertit
une slice NDXR verifiee en samples natifs bornes utilisables pour l'etape
suivante : faire porter la couverture render target par de vraies donnees
geometry au lieu d'une surface synthetique par primitive.
