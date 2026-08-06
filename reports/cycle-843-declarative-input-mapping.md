# Cycle 843 — declarative input mapping boundary

Date: 2026-08-04

## Résultat

`InputMappingDatabase` charge des bindings `button_mask<TAB>event` et résout
les événements explicites (`start_mission`, `pause`, `resume`, `complete`,
`abort`) par masque exact. Les masques nuls, doublons, valeurs hors 16 bits,
événements inconnus et manifestes vides sont rejetés.

Le produit ne contient aucun code SDL ni valeur de bouton PAL en dur : le
manifeste qualifié reste la source des mappings, ce qui garde la frontière
plateforme séparée du scénario.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

Le raccord SDL3 qui convertit les événements physiques en `InputFrame` et
charge un manifeste retail qualifié reste à faire. Ce cycle ne choisit donc
aucun mapping PAL par défaut.
