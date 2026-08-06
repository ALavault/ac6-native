# Cycle 801 — scheduler fixe et replay déterministe

L’API native est maintenant `MissionRuntime::tick(fixed_dt, InputFrame)`.
Elle intègre les commandes dans un état de simulation stable à pas fixe et
retourne un `WorldFrame` avec transform déterministe. Deux runtimes rejouant
120 ticks identiques produisent exactement les mêmes positions ; les assets
restent la seule condition de `mission_ready`.

Validation : build CMake, CTest `1/1`, démarrage sans manifeste toujours
fail-closed (code 2).
