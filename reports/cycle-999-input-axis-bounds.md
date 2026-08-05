# AC6 cycle 999 — bornes des axes de vol

Date : 2026-08-05

## Correction

`MissionRuntime` normalise désormais les axes signés SDL par division par
`32767` puis saturation dans `[-1, 1]`. Le cas asymétrique `-32768` ne peut
plus produire une valeur inférieure à `-1`; le throttle conserve son domaine
octet `[0, 255]`.

## Preuve native

La fixture `product_runtime_tests` lance deux exécutions Mission 1
indépendantes et vérifie les trois axes à `-32768` puis à `32767`. Les valeurs
de simulation obtenues sont exactement `-1/60` et `+1/60`. Ce résultat est une
preuve native de bornage, pas une observation stock, bridge ou oracle Xenia.

## Validation

- `git diff --check`
- `cmake --build build -j2`
- `SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir build --output-on-failure`
- Résultat : 5/5 tests passés.

Frontière : normalisation input dans le runtime natif. Aucun C++ généré,
conteneur PAC ou service Xbox/XMA n’a été modifié.
