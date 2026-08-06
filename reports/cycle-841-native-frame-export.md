# Cycle 841 — native framebuffer export

Date: 2026-08-04

## Résultat

Le framebuffer `NativeRenderTarget` peut maintenant être exporté en PPM P6
depuis le harnais de vérification (`AC6_NATIVE_FRAME_DUMP=/path/frame.ppm`).
L'export est borné à la cible déjà rendue et n'est pas appelé par
`ac6-native`; il ne constitue donc pas une présentation SDL/Vulkan.

Artefacts de la fixture 64×32 :

- [PPM](cycle-841-native-frame.ppm), SHA-256
  `f7de1a40a68644c73360b7e0da3817d6a33140dd1ff87cd96e66420b86739c16` ;
- [PNG de consultation](cycle-841-native-frame.png), SHA-256
  `f925a09d51f9d4a92c116b37f80846f14c1cb7b910b9d6bae1f3f934eb9a3c51`.

L'image est une fixture synthétique de géométrie qualifiée, principalement
noire à cette résolution. Elle ne doit pas être présentée comme une capture
retail Mission 01.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
AC6_NATIVE_FRAME_DUMP=... ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
file reports/cycle-841-native-frame.ppm                            64 x 32 P6
```

Le test vérifie aussi l'en-tête PPM, la taille exacte des pixels et le rejet
d'un chemin vide. L'export ne modifie ni les hashes de readback ni le contrat
de présentation produit.

## Limite restante

Une capture visible de `ac6-native` reste ouverte : il faut encore raccorder
le `WorldFrame` au backend SDL3/Vulkan et à une fenêtre/swapchain native.
