# Cycle 844 — input mapping to scenario dispatch

Date: 2026-08-04

## Résultat

`MissionScenario::dispatch_buttons` relie un masque résolu par
`InputMappingDatabase` à un `Event` explicite. Le sujet est transmis au HSM;
un masque absent ne produit aucun événement et retourne `false`.

La fixture démarre une mission, met le scénario en pause via le binding
déclaratif, reprend via le binding suivant, puis vérifie le rejet d'un bouton
non mappé. Aucun état de scénario n'est choisi par `mission_id`.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

Le service SDL3 qui fournit les masques physiques et les axes n'est pas encore
branché; les valeurs PAL restent à qualifier avant d'ajouter un manifeste
retail.
