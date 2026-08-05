# AC6 cycle 998 — contrats vagues et IA

Date : 2026-08-05

## Portée

Le runtime natif générique accepte désormais deux services optionnels dans le
manifeste runtime :

- `waves` : douze colonnes `mission_id`, `spawn_tick`, `unit_id`, `owner`,
  `asset`, `faction`, position XYZ, santé, santé maximale et rayon de
  collision ;
- `ai` : six colonnes `mission_id`, `first_tick`, `period_ticks`, `entity`,
  `target` et `weapon_id`.

Chaque parseur exige le nombre exact de champs, rejette les valeurs invalides,
les doublons et un manifeste vide, puis publie la base seulement après le
dernier enregistrement valide. `MissionRuntimeServices` conserve les deux
directeurs avec leurs indicateurs `has_*`; `MissionExecution` et les routes
CLI `services-smoke`/`present-manifest` consomment ces instances directement.

## Preuve native

La fixture multi-mission charge les familles air-intercept, strike et escort.
Mission 1 ajoute une vague `5000` au tick 1 et une règle `4097 → 5000` avec
l’arme 7. Le premier tick publie l’unité active et le projectile; le second
déclenche la radio séquencée. La campagne termine ensuite en succès. Un échec
de service conserve l’ancien bundle grâce à la publication temporaire globale.

Cette fixture est une reproduction native du contrat. Aucun paramètre n’est
présenté comme observation stock, bridge ou oracle Xenia; les paramètres retail
de vagues, IA et timings restent à qualifier depuis les sources binaires.

## Validation

- `git diff --check`
- `cmake --build build -j2`
- `SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir build --output-on-failure`
- Résultat : 5/5 tests passés.

Frontières : loader/runtime natif et fixtures uniquement. Aucun C++ généré,
conteneur PAC complet ou service XMA/Xbox n’a été modifié.
