# AC6 cycle 1000 — checkpoint des unités et vagues

Date : 2026-08-05

## Correction

`MissionExecution::Checkpoint` persiste désormais le registre `UnitRecord` et
le snapshot ordonné des spawns `MissionWaveDirector`, y compris leur drapeau
`published`. La restauration valide les identités, rétablit le registre et le
combat de façon transactionnelle, puis restaure les vagues sans les republier.

`AC6SESS` passe en version 8 pour sérialiser ces champs. Les versions 1 à 7
restent acceptées en lecture; leurs checkpoints hérités conservent le chemin
legacy sans inventer d’état de vague absent.

## Preuve native

La fixture Mission 1 fait apparaître l’unité 5000 au tick 2, sauvegarde un
checkpoint, la despawn, puis restaure et vérifie trois unités actives, une
vague publiée et aucune vague en attente. La fixture de sauvegarde roundtrip
également une vague non publiée et deux `UnitRecord`.

## Validation

- `git diff --check`
- `cmake --build build -j2`
- `SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir build --output-on-failure`
- Résultat : 5/5 tests passés.

Frontières : état runtime natif et format `AC6SESS`; aucun C++ généré,
conteneur PAC ou service Xbox/XMA n’a été modifié.
