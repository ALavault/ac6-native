# Cycle 832 — native camera projection target

Date: 2026-08-04

## Resultat

La voie `draw_world_geometry` ne projette plus les donnees world-space par
formules modulo. Elle construit maintenant une projection caméra native depuis
`WorldFrame.camera_*`.

La projection :

- derive `origin`, `forward`, `right`, `up` depuis camera et target ;
- rejette une caméra degeneree ;
- applique un near plane `0.1` ;
- applique un FOV vertical borne via focal fixe ;
- rejette les points hors frustum ;
- convertit les points visibles en pixels target.

Les samples vertex/index et les deux points bounds sont marques uniquement
apres transformation locale -> world-space puis projection caméra.

## Guards ajoutees

`draw_world_geometry` echoue si :

- la caméra est non finie ou degeneree ;
- aucun sample/bounds projetable n'est visible ;
- un point projeté est derriere le near plane ;
- un point sort du frustum.

Les index restent valides contre `vertex_count`; seul un index qui pointe vers
un vertex sample disponible est projeté.

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

## Tests ajoutes ou ajustes

- transforms fixtures placees dans le cone de la caméra de suivi ;
- couverture geometry non nulle et hashes differents du fallback synthetique ;
- hashes reproductibles avec transforms identiques ;
- hashes differents avec transforms modifiees ;
- rejet d'une frame avec `camera_target == camera_origin`.

## Limites

Cette projection n'est pas encore la matrice retail/Xenos. Elle ferme un
contrat produit utile : les pixels monde geometry-driven dependent maintenant
de la caméra native, de la transform drawable et des samples geometry bornes,
avec clipping fail-closed.
