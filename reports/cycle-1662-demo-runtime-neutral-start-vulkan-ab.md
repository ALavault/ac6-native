# Cycle 1662 — A/B runtime Vulkan neutral/START

## Résultat

Deux probes codegen ON, depuis le store PAL qualifié, ont été exécutés avec le
même binaire et le même chemin Vulkan jusqu’au tick 253 : neutral, puis START
(`XINPUT_GAMEPAD_START=0x0010` au tick 252). Les deux runs terminent sur
`max_ticks` (exit 4 attendu), sans frontend qualifié.

Le rapport `graphics` est byte-identique sur les deux routes : `116`
notifications de présentation, `5` shader loads, `26` draws, `1` present,
`1` normal draw et `1` neutral resolve. Les IB restent strictement identiques :

| IB | longueur | neutral | START |
|---|---:|---|---|
| `0x127CA0C0` | 11 dwords | `ef7ab6e4…d2b0` | `ef7ab6e4…d2b0` |
| `0x1274A000` | 3029 dwords | `d121c8d8…358d6` | `d121c8d8…358d6` |

Le readback normal traduit est all-zero et identique dans les deux routes
(`0b150fd3…ec58366`, 640×360). Le resolve neutral 1280×720 est également
identique (`0c660f2b…a4913a5f`). À ce checkpoint, START est donc un événement
d’entrée observé, mais aucune transition guest et visuelle causale n’est
prouvée; il reste volontairement non promu.

## Limites

Les traces binaires diffèrent seulement par l’événement d’entrée et les
événements de contrôle associés. Cette A/B ne qualifie pas une destination
guest-owned : le resolve utilise toujours la projection neutre 1× explicitement
gardée par le readback normal noir. Le contenu EDRAM RT0, les pixels non noirs,
le frontend et la mission restent ouverts. Aucune screencap n’est produite.

## Vérifications

- CTest codegen ON : `17/17`.
- CTest démo OFF : `18/18`.
- Identité PAL vérifiée dans les deux rapports :
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
- Traces temporaires : neutral SHA `c5357c6d…b1c5794`, START SHA
  `4a7326d9…e25724f`; rapports SHA `33b6c8b3…5685a7` et
  `2d0c391b…b91a6cf`.

Capsule durable : `analysis/demo/ac6-demo-runtime-neutral-start-vulkan-ab-v1.json`.
