# Cycle 849 — adaptateur d'input SDL3

Date: 2026-08-04

## Résultat

Un module `ac6_platform` séparé du cœur traduit les événements SDL3 de
gamepad en `InputFrame` : quatre axes configurables, inversion explicite,
normalisation du throttle vers 0–255, et masque de boutons 16 bits.

Un bouton pressé résout le binding déclaratif courant et produit un `Event`
avec son sujet; un bouton relâché efface seulement le masque. Les axes
inconnus, boutons hors plage et mapping invalide sont rejetés. Le cœur
`ac6_product_core` ne dépend toujours pas de SDL3; seul `ac6_platform` et les
exécutables de validation sont liés à SDL3.

## Validation

```text
cmake -S reconstruction/ace-combat-6 -B reconstruction/ace-combat-6/build OK
cmake --build reconstruction/ace-combat-6/build -j2                       OK
ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

La fixture synthétique couvre pitch inversé, throttle maximal, bouton
`StartMission`, sujet transmis et relâchement.

## Limite

Il manque encore la boucle `SDL_PollEvent`, l'ouverture/fermeture des
périphériques et le manifeste PAL réellement qualifié. Ce module ne crée pas
encore de fenêtre ni de swapchain Vulkan.
