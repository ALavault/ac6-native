# Cycle 922 — scheduler de simulation à pas fixe

`MissionRuntime::tick` accumule désormais les durées appelant jusqu’à un
maximum borné de huit pas de simulation à 60 Hz. La physique ne dépend plus
directement d’un `fixed_dt` variable; les appels sub-tick et multi-tick sont
rejouables, et la pause n’accumule pas de temps. Une restauration de snapshot
réinitialise l’accumulateur pour garder le replay déterministe.

Le test vérifie explicitement `1/120 + 1/120 -> 1 tick` et `1/30 -> 2 ticks`.

Validations : CTest normal 3/3, ASan/UBSan 3/3, package audit pass.
