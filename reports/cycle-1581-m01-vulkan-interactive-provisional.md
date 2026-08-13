# Cycle 1581 — M01-B interactive Vulkan upload path

## Résultat

Le chemin `ac6-native play` normal ne construit plus de `NativeRenderTarget`.
Il ouvre une scène Mission 01 depuis le cache PAL, convertit un draw map
qualifié (NDXR + texture NTXR) en ressources Vulkan persistantes, rend dans une
cible offscreen Vulkan et envoie le readback RGBA8 au swapchain via le buffer de
staging permanent. La cible CPU reste limitée à `--scene-capture`, explicitement
diagnostic.

Les slices utilisées sont celles de
`analysis/assets/mission01-pal-native-extraction.v1.json`; aucun conteneur PAC,
tracker, tracking ou telemetry n'est copié dans le dépôt.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j16              pass
product_boundary (source + ac6-native)                            pass
ac6-vulkan-backend / scene-renderer / resource-cache               pass
ac6-vulkan-surface-smoke sous Xvfb + SDL_AUDIODRIVER=dummy         pass
play PAL --frames 1, deux processus propres                         pass
readback PPM SHA-256                                                0ad652691d037a7ea76e3367312d88f28cfcb3b09f0cd430305a7af65de554fd
présence pixels non noirs                                           554880 octets non nuls
```

## Frontière maintenue

Le renderer est encore **provisional-covered** : le backend Vulkan offscreen
et le device swapchain sont distincts, donc le dernier transfert est un
readback/upload et non une soumission directe des `DrawPacket` au swapchain.
Le HUD GPU, la terrain/les unités complètes, la caméra TCAM publiée et la
qualification JV restent ouverts. Les shaders de cette première tranche sont
les fixtures SPIR-V bornées déjà testées ; ils ne ferment pas la lane
Scene/TCAM.
