# Cycle 1736 — fetches renderer PAL joints, source EDRAM toujours inconnu

## Résultat

Une exécution neutral fraîche, codegen ON, limitée à 253 ticks et instrumentée
uniquement par `AC6_DEMO_WATCH_RESOLVE=1`, a capturé les deux plages de sommets
consommées par les rectangles PM4. Le run est déterministe avec le frontier
Vulkan existant : 5 loads, 26 draws, 1 PRESENT, 4 modules validés et 2
pipelines. Les readbacks restent noirs (`0b150f…` en 640×360 et `0c660f…` en
1280×720).

Le reçu durable est
[`ac6-demo-renderer-input-trace-v1.json`](../analysis/demo/ac6-demo-renderer-input-trace-v1.json).

## Identité et portée

| Élément | Valeur |
|---|---|
| XEX | `Default.xex`, `de917873…5da8` |
| Architecture | Xenon big-endian / Xenos |
| Route | neutral, store neuf, Vulkan, 253 ticks |
| Instrumentation | `AC6_DEMO_WATCH_RESOLVE=1` |
| Binaire codegen ON | `068d3679…c5c1b` |
| Trace RTPLY | `c5357c6d…5794` |

## Fetches observés

| Rôle | Tick | Adresse | Dwords | Endian | Hash des bytes |
|---|---:|---|---:|---:|---|
| rectangle normal | 0 | `0x127CA03C` | 21 | 2 | `cf61dc45…dac7db1` |
| rectangle resolve | 1 | `0x127CA090` | 6 | 2 | `1187ed99…10fa12` |

Les dwords complets sont dans la capsule. Le second fetch est également relu
par la sonde `AC6_RESOLVE_VERTEX`; il s’agit de la même plage, pas d’un second
buffer.

## Classification

- `demo-observed` : bytes des deux fetches, compteurs renderer et hashes de
  readback.
- `demo-qualified` : aucun nouveau pixel ou effet EDRAM.
- `xenia-generic` : aucun nouvel élément utilisé dans ce cycle.
- `unknown` : contenu non nul du RT0 EDRAM avant `RB_COPY`, production des 24
  draws bootstrap et pixels frontend.

La sonde ne fabrique ni EDRAM, ni shader, ni image. Elle confirme que le
prochain test ciblé doit instrumenter le résultat du draw normal dans le
backend Xenos/EDRAM, avant le copy aux offsets PM4 326–387; une nouvelle
résolution avec une source synthétique resterait interdite.

## Validation

Le binaire et le RTPLY sont ceux du run capturé; aucune modification de
Xenia/ReXGlue, Ghidra, microcode ou C++ généré n’est impliquée. Le chemin
production reste fail-closed et aucune screencap n’est promue.
