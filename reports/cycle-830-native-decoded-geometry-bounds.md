# Cycle 830 — native decoded geometry bounds

Date: 2026-08-04

## Resultat

`DecodedGeometry` porte maintenant une bounding box native calculee depuis les
samples vertex acceptes par `NativeGeometryDatabase`.

Champs ajoutes :

- `DecodedGeometryBounds::min_x/min_y/min_z`;
- `DecodedGeometryBounds::max_x/max_y/max_z`;
- `DecodedGeometryBounds::valid`.

## Guards ajoutees

`NativeGeometryDatabase::load_verified` refuse maintenant :

- une position vertex sample non finie ;
- une geometry sample sans bounds valide.

`NativeRenderTarget::draw_world_geometry` refuse maintenant :

- bounds absente ;
- bounds non finie ;
- ordre `min > max`.

La voie geometry-driven marque la cible avec les samples vertex/index et deux
points derives de la bounding box.

## Validation

Commandes executees :

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

Resultat : aucun marqueur Xbox/oracle/PPC liste, dependances Linux standard,
aucun branchement produit Mission 01 ou asset 9/119 hardcode dans `include` ou
`src`.

## Tests ajoutes

- verification des bounds terrain fixture `021/010_NDXR` :
  `min=(0,1,2)`, `max=(3,4,5)`;
- rejet d'un payload NDXR verifie dont le premier float position est NaN.

## Limites

Cette etape ne prouve pas encore les transforms retail ni la projection Xenos.
Elle ferme seulement un contrat natif utile : les samples geometry acceptes
ont des bounds finies, deterministes et consommables par la cible render.
