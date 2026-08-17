# Xenia Edge — bridge agent, cycle 1

Date : 2026-08-16

## Identité

- dépôt : `https://github.com/has207/xenia-edge.git`
- branche distante : `edge`
- commit : `e4b13738c3c461b2c06241fa3f54b5a669b6a304`
- checkout local isolé : `.tools/xenia-edge-source`
- cible AC6 du moteur externe : démo PAL `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`

## Résultat

Le premier seam du bridge est implémenté dans le checkout Xenia Edge :

- état manette indépendant du backend dans `src/xenia/base/agent_bridge.*` ;
- compteur monotone au boundary commun situé immédiatement après le retour de
  `IssueSwap` pour le packet `XE_SWAP` ;
- durée d'action exprimée en présentations invitées terminées ;
- identifiants d'action strictement croissants, rejet des chevauchements et
  neutralisation à expiration ;
- injection au niveau `InputSystem::GetState`, donc au poll invité XAM et avant
  toute dépendance SDL/HID hôte ;
- option `agent_bridge`, désactivée par défaut ;
- trois tests unitaires couvrant durée, ordre, slot, rejet et capacités.

Le code ne prétend pas encore fournir une pause globale exacte. Le manifeste
reste fail-closed : seule `guest_controller_injection` est vraie ; pause après
présentation, frame-step exact, capture framebuffer en pause et restauration
de checkpoint restent fausses.

## Validation

- compilation isolée Clang C++20 avec `-Wall -Wextra -Werror` ;
- 3 cas / 27 assertions : PASS ;
- `clang-format --dry-run --Werror` : PASS ;
- `git diff --check` : PASS.

Empreintes :

| Fichier | SHA-256 |
|---|---|
| `src/xenia/base/agent_bridge.h` | `8c26f2b6045f91abd27daeb3eef139087ae8611dae733351155180acd15a4ff5` |
| `src/xenia/base/agent_bridge.cc` | `1199d85f0fb6bd2d84e4978e1a8eb266ba91a797fae58b3077c9611c53f4cff2` |
| `src/xenia/base/testing/agent_bridge_test.cc` | `2f0d90fed06924045d9a9da2911e8d81bdae39a6c2dfa05ee2589f6f704a2dc6` |
| `src/xenia/gpu/command_processor.h` | `e0b15b4dbd7cd9fa512f101eac9ed7ae688abe26c3d683b7df8274da9c50c7e8` |
| `src/xenia/gpu/pm4_command_processor_implement.h` | `f8a0527e01714ec45e15f1f536f32db4c93ab443f663b2c19f9cd254181add3c` |
| `src/xenia/hid/input_system.cc` | `11e21a80daad25d110870334168410259abf2ab19e10d790b5a89fcf96844acb` |

## Frontière suivante

Ajouter un transport local authentifié par chemin explicite, publier
l'observation et attendre l'action sans bloquer le thread GPU dans une pause
réentrante. La coordination doit suspendre les vCPU invités avant la capture,
laisser le thread de contrôle hôte actif, puis reprendre exactement le nombre
de présentations demandé. Ensuite seulement, relier capture framebuffer et
`Emulator::SaveToFile` / `RestoreFromFile`.

Une build Xenia complète et un A/B AC6 restent nécessaires : le test actuel
valide le state machine isolé, pas encore la synchronisation de tous les
threads invités.
