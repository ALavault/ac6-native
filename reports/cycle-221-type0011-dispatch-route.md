# AC6 cycle 221 — routage du dispatcher des records

## Identité et preuves

- Cible : `ac6-xbox360-pal`
- Module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Exports headless : `exports/8211bcd8.json`, `exports/8211bb80.json`,
  `exports/82339798.json`
- Dispatcher retail : `0x8211bcd8`

Le dispatcher lit le tag demi-mot à `record+0x0a` et route exactement :

- `0x0011` vers `0x82339798` ;
- `0x8181` vers `0x8211bb80` ;
- tout autre tag vers un retour sans traitement.

## Implémentation native bornée

`route_function_8211bcd8` expose ce routage avec une énumération explicite.
Les actions restent dans leurs contrats spécialisés : le chemin `0x0011`
dispose maintenant de la dématérialisation des cycles 220/218, tandis que le
corps inverse complet `0x8181` reste une frontière séparée. Une adresse nulle
est traitée comme un no-op; une lecture hors plage renvoie `nullopt`.

## Validation

Le test `ac6-motion-record-tests` vérifie les trois tags, le record nul et le
maintien de la séparation avec le corps `0x8181`.

```bash
cmake --build .build/ace-combat-6/native -j16 --target ac6-motion-record-tests
ctest --test-dir .build/ace-combat-6/native --output-on-failure \
  -R '^ac6-motion-record-tests$'
cmake --build .build/ace-combat-6/native -j16
ctest --test-dir .build/ace-combat-6/native --output-on-failure -j16
cmake --install .build/ace-combat-6/native --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

Résultat : test ciblé **1/1 PASS**, corpus AC6 **42/42 PASS**. Aucun Xenia,
GPU, GUI, VNC ou geste humain n'a été utilisé.

## Frontière

Le routage ne donne aucun nom métier au record et ne démontre pas qu'il
représente un avion, une caméra ou une mission. Le corps inverse `0x8181` et
les consommateurs de vol restent `needs-types` / `needs-dynamic-evidence`.
