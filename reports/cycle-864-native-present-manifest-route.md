# Cycle 864 — route `ac6-native` manifest → renderer → present

Date: 2026-08-04

## Résultat

`ac6-native --present-manifest <manifest> <mission_id>` charge runtime et
render, vérifie et décode chaque buffer référencé, lance `MissionExecution`,
produit un `WorldFrame`, rend via `VulkanRenderer`, puis présente la cible
native par SDL/Vulkan. Les erreurs sont séparées par étape et fail-closed.

Le mode `--present-smoke` et le démarrage headless restent disponibles; aucun
asset retail n'est embarqué. La route manifestée attend donc un jeu complet de
manifests et buffers fourni localement.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
ac6-native                                                        exit 0
xvfb-run -a env SDL_AUDIODRIVER=dummy ac6-native --present-smoke    exit 0
ac6-native --present-manifest /tmp/no-such-ac6-manifest 1           exit 9
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
```

## Limite

La preuve positive Mission 01 attend encore le manifeste render qualifié et
les buffers retail disponibles localement; aucun chemin synthétique ne les
remplace dans `--present-manifest`.
