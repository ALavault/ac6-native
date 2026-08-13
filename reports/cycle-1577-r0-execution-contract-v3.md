# Cycle 1577 — R0 aligne l'oracle et la trace d'exécution v3

Date : 2026-08-13.

## Résultat

La colonne vertébrale M01 sépare maintenant trois identités sans promotion
implicite : oracle comportemental NTSC-U/J `6eefba42…cbbbc` sur
`AC6_recomp@ab90b547…`, cible native PAL `acc302c1…bcde`, et ancienne lignée
PAL `dcd41b…` historique seulement. Le checkpoint 2 reste ouvert à 0/6 lanes.

`tools/build_ac6_execution_trace_v3.py` écrit exclusivement
`ac6.execution-trace.v3`. Son header scelle l'identité et le marqueur oracle,
un reçu inter-région v4, la cible PAL, la pile de patches, le binaire, le
manifest de build, le probe, le replay et la capture. Chaque tick contient,
dans l'ordre, `input`, `simulation`, `objectives`, `graphics`, `media`,
`hashes`. Le lecteur accepte v2 avec la classification `historical-v2`; il ne
peut ni l'émettre ni la promouvoir.

Le handoff actif est `AC6_RECOMP_LINUX_ORACLE_HANDOFF.md`. Le document Xenia
reste applicable seulement à une frontière statique explicitement demandée.

## Validation

- build natif `-j16` : pass ;
- CTest sous Xvfb et `SDL_AUDIODRIVER=dummy` : 87/87, zéro échec ;
- dette cache explicite : 4 skips (`retail-counter-corpus`, `retail-scene-tcam`,
  `retail-frontend-resources`, `retail-mission01-vulkan-scene`) ;
- tests Python : Pytest 284/284 avec 37 subtests, et `unittest` 176/176 ;
  tests trace v2/v3 ciblés : 37/37 ;
- Ruff `tools scripts` : pass ;
- audits ladder, checkpoint 2 et reproductibilité oracle : pass ;
- `git diff --check` : pass.

## Risques résiduels

O1 n'est pas commencé : le runtime moderne Linux/Vulkan n'est pas construit,
la cadence n'est pas qualifiée et aucune capture v3 n'existe. Les quatre tests
retail ignorés devront repasser avec le cache avant la validation terminale.
R0 ne ferme ni JV, ni JP, ni lane du checkpoint 2.
