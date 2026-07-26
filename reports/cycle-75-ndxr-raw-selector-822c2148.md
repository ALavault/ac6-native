# Cycle 75 — sélecteur brut NDXR `Function_822C2148`

## Evidence

- cible : AC6 PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- module/adresse : Xbox 360 PPC `0x822C2148` ;
- source : export headless [`exports/822c2148.json`](../exports/822c2148.json).

L'export accepte un flux NDXR/NDP3 direct ou un préfixe GIDX de quatre mots.
Après le préfixe éventuel, il vérifie les octets `+8 == 2` et `+36 == 0`,
calcule `(+index + 1) * 0x30`, rejette le flag `+0x26 & 4`, puis copie trois
mots vers une première sortie et le quatrième vers une seconde.

## Native boundary

`function_822c2148_select_raw_record` complète le parseur NDXR existant sans
le remplacer : le parseur haut niveau décrit le format, tandis que cette
primitive conserve le protocole exact de sélection XEX, les marqueurs GIDX /
NDXR / NDP3 et les mots invités big-endian. Les quatre mots restent opaques.

Les tests couvrent un NDXR direct, le rejet du flag, et le chemin GIDX avec
copies de mots distinctes. Ils ne qualifient pas une géométrie, une ressource,
ou le rendu Xenos.

## Validation

```bash
cmake --build .build/ace-combat-6/native -j16 --target ac6-ndxr-tests
ctest --test-dir .build/ace-combat-6/native --output-on-failure \
  -R '^ac6-ndxr-tests$'
ctest --test-dir .build/ace-combat-6/native --output-on-failure -j16
cmake --install .build/ace-combat-6/native --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

Le test ciblé passe **1/1** et le corpus AC6 complet passe **41/41**.
