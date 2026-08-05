# AC6 cycle 1001 — synchronisation activité combat/registre

Date : 2026-08-05

## Correction

Après chaque tick de gameplay, `MissionExecution` recopie l’état actif des
`CombatUnitState` vers le `UnitRegistry` avant de publier `WorldFrame`. Une
destruction par projectile ou désactivation ne laisse donc plus un ennemi
mort compté dans `active_units` ni dans les checkpoints suivants.

## Preuve native

La fixture d’arme Mission 1 tire deux fois sur l’entité 4098, vérifie sa santé
nulle, puis vérifie `WorldFrame.active_units == 1` et
`UnitRegistry::active_count() == 1`. Les routes de vague et de restauration
restent couvertes par le même test.

## Validation

- `git diff --check`
- `cmake --build build -j2`
- `SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir build --output-on-failure`
- Résultat : 5/5 tests passés.

Frontière : cohérence d’état du runtime natif. Aucun C++ généré, conteneur PAC
ou service Xbox/XMA n’a été modifié; ce n’est pas une preuve stock/bridge/Xenia.
