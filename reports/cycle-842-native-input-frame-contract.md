# Cycle 842 — input carried by `WorldFrame`

Date: 2026-08-04

## Résultat

`WorldFrame` conserve maintenant l'`InputFrame` effectivement consommé au
tick. La dynamique de vol n'est pas modifiée : le champ rend la trace
entrée→simulation→rendu vérifiable et permet de distinguer un axe nul d'un
axe non lu.

La fixture couvre :

- frame initiale avec pitch/roll/yaw/throttle/boutons nuls ;
- 120 ticks avec axes et throttle non nuls, identiques sur deux replays ;
- retour à une frame neutre, avec les cinq champs revenus à zéro ;
- égalité des poses et ticks entre les deux exécutions déterministes.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
reconstruction/ace-combat-6/build/ac6-native                       exit 0
```

## Limite

Le contrat expose les commandes reçues mais ne prétend pas encore reproduire
les courbes de contrôle retail, les boutons de pause/caméra ou les mappings
SDL3 qualifiés. Ces éléments restent dans le prochain contrat input/platform.
