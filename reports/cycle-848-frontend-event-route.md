# Cycle 848 — frontend piloté par événements

Date: 2026-08-04

## Résultat

`FrontendController` accepte maintenant un `Event` explicite et un masque via
`InputMappingDatabase`. `StartMission` avance d'une étape frontend; `Abort`
réinitialise l'état et la sélection au titre. Les événements de gameplay
(`Pause`, `Resume`, etc.) ne sont pas consommés par le frontend.

La fixture traverse naturellement Title → New Game → Briefing → Hangar →
Loading → Mission avec cinq confirmations identiques, puis vérifie abort →
Title. Le parcours ne choisit aucun `mission_id`.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
reconstruction/ace-combat-6/build/ac6-native                       exit 0
```

## Limite

Le frontend n'est pas encore piloté par une boucle SDL3 réelle et ne charge
pas encore le manifeste PAL depuis un chemin utilisateur; ce cycle ferme le
contrat d'événements générique.
