# Cycle 859 — orchestrateur d'exécution Mission

Date: 2026-08-04

## Résultat

`MissionExecution` centralise désormais le contrat
`MissionDefinition + MissionAssetDatabase + MissionLaunchDefinition` :
construction du scénario, enregistrement/activation des unités, binding du
joueur, dispatch `StartMission`, puis `MissionRuntime::tick` vers un
`WorldFrame`. Un lancement invalide remet l'état à zéro et ne publie pas de
session partielle.

La fixture utilise les assets 9/119/165/199/210, deux unités et le joueur
4097; le premier tick produit `mission_ready`, deux unités actives et le joueur
attendu. Le code ne contient aucun branchement spécifique à Mission 01.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

`ac6-native` n'utilise pas encore cet orchestrateur : il faut lui fournir les
manifestes qualifiés et les bases géométrie/matériaux nécessaires avant de
présenter une frame Mission 01 réelle.
