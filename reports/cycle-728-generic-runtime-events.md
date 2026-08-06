# Cycle 728 — dispatch générique des événements de progression

Date : 2026-08-04  
Périmètre : remplacer la mutation directe des objectifs par un point de
dispatch commun aux producteurs SDL, replay et futurs événements invités.

## Contrat ajouté

`CampaignRuntimeEvent` porte un type borné (`objective_completed` ou
`mission_completed`) et un index d'objectif. `apply_campaign_runtime_event()`
rejette les combinaisons invalides sans mutation; la complétion de mission ne
peut pas contourner les objectifs requis. Le runtime reste indépendant de SDL,
Vulkan et de l'ABI Xbox.

La fixture SDL utilise maintenant ce dispatch pour terminer Mission 1 après la
présentation, tandis que le test runtime vérifie explicitement qu'un événement
de mission prématuré est rejeté, puis que deux événements d'objectif suivis de
l'événement de mission font passer `active → complete`. Le résultat SDL reste
identique : `mission1_completed=1`, restauration `AC6S`, déverrouillage et
présentation Mission 2.

## Validation

```text
build : cmake --build .../reconstruction-material -j2
runtime + SDL Xvfb : succès
targeted CTest : 4/4, 1 skip contrôlé sous dummy, 0.10 s
full CTest PAL : 63/63, 1 skip contrôlé sous dummy, 61.80 s
```

Cette étape ferme le point d'injection générique des objectifs; elle ne
prétend pas encore produire des événements depuis les collisions, le monde ou
la logique de mission réelle. Mission 2 est présentée mais ni volée ni
complétée, et `flight_world_pixels=0` reste la frontière graphique active.

## Hashes

```text
include/ac6/campaign_runtime.h                1eec85c10a60a7259af890ed08dc4ec32ab4de1c1cd17b9fa0ee7c5e2eea3561
src/campaign_runtime.cpp                      df1e1b249a756a9916e4b5ffc44b157a04c773dbb2a87ec597159e0a1c5851dd
tests/campaign_runtime_tests.cpp              4eac5fe15ed3aa53f67c43d23e4c337821ec8374f6aa49d84bc4e96fa7aed624
tests/campaign_vulkan_sdl_present_tests.cpp   0fff53c88be2cac58e4a43e9263c1a87b48c35fa3860cc8968b99d35532fdb03
```
