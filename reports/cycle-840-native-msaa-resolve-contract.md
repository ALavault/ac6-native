# Cycle 840 — native MSAA resolve contract

Date: 2026-08-04

## Resultat

La route render-target intermediaire porte maintenant un contrat de samples.

Le manifeste RT inclut `sample_count` :

```text
mission_id<TAB>target_id<TAB>width<TAB>height<TAB>sample_count<TAB>color_format<TAB>depth_format<TAB>depth_enabled
```

La fixture Mission 01 qualifie deux surfaces :

- `world_color` : `64x32`, `sample_count=4`, `rgba8`, `d24s8`;
- `present` : `64x32`, `sample_count=1`, `rgba8`, `none`.

Le resolve nominal est maintenant `world_color -> present` avec mode
`msaa_resolve`.

## Guards ajoutees

`MissionRenderTargetDefinition` rejette les sample counts non supportes. Les
valeurs acceptees sont `1`, `2`, `4` et `8`.

`VulkanRenderer` transmet la target destination a `draw_world_geometry`, qui
verifie maintenant :

- mission, target id, dimensions et format de la surface source;
- mission, target id, dimensions et format de la surface destination;
- destination sans depth;
- `msaa_resolve` uniquement depuis une source multisample vers une destination
  single-sample;
- rejet d'un `copy` direct quand source et destination n'ont pas le meme
  `sample_count`.

Les sample counts source et destination participent au hash de shading afin
qu'un changement de contrat soit observable dans le readback.

## Tests ajoutes

- chargement RT avec `world_color` 4x et `present` 1x;
- rejet d'un RT avec `sample_count=3`;
- resolve nominal `msaa_resolve`;
- rejet d'un `copy` 4x -> 1x;
- conservation des divergences de hash pour pass et mode resolve.

## Validation

Commandes executees :

```text
cmake --build reconstruction/ace-combat-6/build -j2
ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
strings reconstruction/ace-combat-6/build/ac6-native | rg -i 'xbox|xam|xma|xenia|rexglue|xenonrecomp|ppc'
ldd reconstruction/ace-combat-6/build/ac6-native
rg -n 'mission_id_ == 1|mission_id == 1|assets\.has\(9\)|assets\.has\(119\)' reconstruction/ace-combat-6/include reconstruction/ace-combat-6/src
```

Resultats :

- build CMake OK;
- CTest OK : `1/1`;
- aucun marqueur Xbox/oracle/PPC dans `ac6-native`;
- dependances dynamiques Linux standard seulement;
- aucun branchement produit Mission 01 ou asset 9/119 hardcode dans
  `include` ou `src`.

## Screencap

Pas de screencap retail exploitable a ce stade. `ac6-native` ne cree pas encore
de fenetre SDL/Vulkan; le chemin rendu courant est un framebuffer natif teste
par readback/hash. Une image exportee ou une capture fenetree demande encore
un dump framebuffer ou le raccord SDL/Vulkan du produit.

## Limites

Ce cycle ne qualifie pas encore les formats MSAA retail exacts Xenos, les
resolve shaders/permutations retail ni une presentation fenetree Linux. Il
ferme seulement le contrat natif fail-closed entre surface monde multisample,
surface `present` single-sample et mode de resolve explicite.
