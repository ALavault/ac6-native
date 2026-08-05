# Cycle 997 — contrat de séquence mission

Le runtime charge désormais un manifeste `sequence` borné à six colonnes:
mission, tick, ordre, type, identifiant et durée. Les quatre types génériques
correspondent aux transitions d'objectif et de radio déjà exposées par
`MissionExecution`. Le parseur refuse les types inconnus, les durées
incompatibles, les doublons d'ordre et les séquences vides; la publication
reste transactionnelle.

`MissionRuntimeServices` transporte la séquence avec input, objectifs, radio
et campagne. Le test fixture charge deux événements, les publie aux ticks 1 et
2, observe l'objectif actif et la radio en lecture, puis termine la mission.
Les commandes `services-smoke` et `present-manifest` transmettent également la
séquence au runtime.

Validation:

```text
cmake --build build -j2                         pass
SDL_AUDIODRIVER=dummy xvfb-run -a ctest ...     5/5 pass
```
