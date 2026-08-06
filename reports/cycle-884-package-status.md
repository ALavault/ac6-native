# Cycle 884 — statut du paquet explicite

Le TGZ inclut désormais un README `developer preview` qui interdit toute
interprétation retail : aucune archive ni asset n’est inclus, et une release
Mission 01 exige les manifestes externes complets, `--validate-manifest` et un
`--present-manifest` positif avec comparaisons de frames/replay.

Le paquet contient uniquement README, headers et `bin/ac6-native`. CTest `2/2`
et smoke Vulkan passent.
