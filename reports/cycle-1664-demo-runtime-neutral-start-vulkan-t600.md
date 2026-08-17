# Cycle 1664 — A/B Vulkan neutral/START jusqu’au tick 600

## Résultat

Le build codegen ON courant a été rejoué depuis deux stores neufs, avec le
backend Vulkan et le même XEX PAL (`Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`). La route
START injecte `XINPUT_GAMEPAD_START=0x0010` au tick 252; la route neutral ne
injecte aucune touche.

Les deux probes atteignent exactement 600 ticks et terminent par
`max_ticks` (code retour 4 attendu). Aucun frontend, résultat de mission ou
terminal n’est qualifié.

| propriété | neutral | START |
|---|---:|---:|
| ticks complétés | 600 | 600 |
| notifications PRESENT | 463 | 463 |
| shader loads | 5 | 5 |
| draws | 26 | 26 |
| presents Vulkan | 1 | 1 |
| normal draws | 1 | 1 |
| neutral resolves | 1 | 1 |
| readback normal | `0b150fd3…ec58366` | identique |
| resolve 1280×720 | `0c660f2b…a4913a5f` | identique |

Les deux IB restent identiques :

- `0x127CA0C0`, 11 dwords, `ef7ab6e4832aed218b50126464de899ccf0f4bf2eaf26ecfac6371c51671d2b0` ;
- `0x1274A000`, 3029 dwords, `d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6`.

## Qualification

- `demo-qualified` : identité XEX, longueur/hash des deux IB, A/B jusqu’à
  600 ticks, paramètres du draw normal gardé au cycle 1663 et digests Vulkan
  observés identiques.
- `demo-observed` : événement START au tick 252 et 463 notifications PRESENT.
- `unknown` : contenu EDRAM réellement écrit par le guest, destination
  guest-owned, pixels non noirs, transition frontend, mission et résultat.

Le renderer reste fail-closed : le resolve utilise encore la projection
neutral explicitement validée par le readback normal noir; aucun screencap n’est
produit ni promu. L’extension à 600 ticks ne démontre donc pas une transition
causale de START.

## Artefacts et reproductibilité

Traces temporaires sous `/fastdata/lavaulta/tmp/ac6-demo-vulkan-cycle1664-8lQoYG/` :

- neutral RTPLY SHA-256
  `6c34827cc3f9962a7f4042610d69aeb54bbd0165fd5a1c830d341efad07970c7` ;
- START RTPLY SHA-256
  `2a4577f883bbfa31f8740b35e998da10ea42ddba050fa7232e80abe6c71f27cc` ;
- manifests codegen et Ghidra identiques :
  `ae57c868…bda185` et `576fa31e…0086c`.

Sources gardées au moment du run : `src/main.cpp`
`8871b1d3…c782d5`, `src/vulkan_normal_draw.cpp`
`aa225fb7…fd3381` et `include/ac6demo/vulkan_normal_draw.hpp`
`56c77a24…046ed8`.

Aucune modification retail, Xenia/ReXGlue, Ghidra, C++ généré, microcode ou
actif propriétaire n’est impliquée.

Capsule : `analysis/demo/ac6-demo-runtime-neutral-start-vulkan-t600-v1.json`.
