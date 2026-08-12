# Checkpoint 4h — captures oracle v2 non promouvables

Date : 2026-08-12

Deux exécutions du même binaire oracle (`37e0c88d73b917a3b10a11bed44d5c42908032a3a1a0949a652abdec6d6432e7`), du même XEX PAL et de la même route contrôlée ont chacune produit
3 600 ticks / 18 000 événements `ac6.execution-trace.v2` :

| capture | SHA-256 JSONL | résultat |
|---|---|---|
| `/tmp/ac6-controlled-sortie-delayed-v2-20260812` | `913f93414852ddb60f6eb6b8956cd7c5363f13bf07c40d52f3d1e40a0248c9e0` | complète, mais non déterministe |
| `/tmp/ac6-controlled-sortie-delayed-v2-20260812-c` | `40fa3bfd0ddf9f715f70dcfe3908ad3c5fccc0049ad99051d1b9015d49d4e7a2` | complète, mais non déterministe |

La première divergence est au record `sequence=1`, `tick=1`, domaine
`simulation_snapshot` : le mot de transformée joueur/enfant final diffère
(`3304880076` contre `3304879704`). Les entrées restent identiques jusqu'au
record `sequence=590` / tick 119, où le second run reçoit `roll=32767` alors que
le premier est neutre. Les hashes de sortie divergent donc avant toute
comparaison oracle↔natif.

Les manifestes signalent `game_status=-9` (arrêt borné du harness), aucun fatal,
et les deux runs ont le même binaire et la même route. Ils ne sont pas importés
comme oracle qualifié, ni utilisés pour corriger la simulation ou le renderer.
Le comparateur existant reste l'autorité pour la prochaine capture obtenue avec
une source d'entrée rejouée et une frontière de départ identique.
