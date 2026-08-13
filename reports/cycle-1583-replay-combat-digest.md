# Cycle 1583 — replay digest includes combat state

Le `semantic_hash` du replay natif incluait déjà la pose, la caméra et le
curseur de script ; il couvre désormais, à chaque tick, les unités (identité,
faction, position, santé, rayon, activité), les projectiles actifs, les dégâts,
la cible verrouillée et le loadout d'arme. Le rapport déclare la nouvelle base
`world_script_combat_v1`. Les octets du format `AC6RTPLY` ne changent pas : ils
restent l'identité d'inputs, et l'état est recalculé depuis le runtime.

Validation ciblée :

```text
ac6-retail-replay-trace-cadence-tests  pass
build ac6-native + replay-trace       pass
```

Cette garde ne prétend pas encore couvrir IA, objectifs complets, caméra TCAM,
audio ou save ; ces domaines restent à porter dans M01-D/E.
