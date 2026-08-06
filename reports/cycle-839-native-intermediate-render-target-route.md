# Cycle 839 — native intermediate render target route

Date: 2026-08-04

## Resultat

La voie geometry-driven supporte maintenant une surface intermediaire declaree
pour la passe monde.

Le manifeste RT inclut maintenant un `target_id` :

```text
mission_id<TAB>target_id<TAB>width<TAB>height<TAB>color_format<TAB>depth_format<TAB>depth_enabled
```

Le test Mission 01 utilise deux targets :

- `world_color` : `64x32 rgba8 d24s8`;
- `present` : `64x32 rgba8 none`.

La passe `world` cible `world_color`, puis le resolve déclare
`world_color -> present`.

## Guards ajoutees

`MissionRenderTargetDatabase` accepte plusieurs targets par mission et refuse
les doublons par couple `(mission_id, target_id)`.

`VulkanRenderer` vérifie maintenant :

- la RT source nommée par `pass.color_target`;
- la RT destination nommée par `resolve.destination_target`.

`draw_world_geometry` vérifie que la RT source fournie correspond au target de
la passe active.

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

- manifeste RT avec `world_color` et `present`;
- passe `world` ciblant `world_color`;
- resolve `world_color -> present`;
- rejet du rendu si la RT destination `present` est absente.

## Limites

Ce cycle ne modele pas encore les RT intermediaires retail exactes ni copies
Xenos/MSAA. Il ferme le contrat structurel : le rendu monde peut maintenant
passer par une surface intermediaire nommee et resolue explicitement vers la
sortie finale.
