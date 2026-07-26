# AC6 cycle 224 — normaliseur de sous-entrée type `1`

## Identité et preuves

- Cible : `ac6-xbox360-pal`
- Module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Projet Ghidra canonique : `ace-combat-6`
- Fonction retail : `0x82119740`
- Appelant qualifié : `0x82118a50`, branche record `0x8181`
- Export headless : `exports/82119740.json`

L'export montre un contrat borné : un enfant dont le demi-mot `+0x20` n'est
pas `1` retourne zéro sans écriture. Pour le type `1`, `0x82119740` efface
`child+0x34`, reloge `child+0x30`, puis parcourt ses sous-entrées de stride
`0x30`. Chaque sous-entrée reloge ses deux pointeurs `+0x14/+0x18` par rapport
à l'enfant et reçoit deux mots de dispatch sélectionnés par les demi-mots
`+0x0a/+0x0c/+0x0e`. Certains choix posent le bit bas de `+0x08`. Si
`child+0x38` est non nul, le pointeur `+0x3c` est aussi relogé.

Les mots de dispatch sont conservés comme adresses invitées brutes. Aucun nom
d'objet, d'avion, de mission ou de comportement de vol n'est attribué.

## Implémentation native bornée

`normalize_function_82119740_type_1` dans `motion_record.cpp` prévalide toute
la table, les pointeurs, les champs de sélection et le pointeur optionnel avant
la première écriture. Sur une plage invitée indisponible, elle renvoie
`nullopt` sans mutation partielle : cette propriété est une sécurité de l'hôte,
pas une prétention sur le traitement retail d'une mémoire XEX corrompue.

Le wrapper `materialize_function_82118a50_type_8181` reste volontairement
séparé : le helper est disponible, mais son intégration doit être une
composition explicite et testée afin que les effets de dispatch ne soient pas
ajoutés silencieusement à un appelant existant.

## Validation

```bash
cmake -S reconstruction/ace-combat-6 -B .build/ace-combat-6
cmake --build .build/ace-combat-6 -j16 --target ac6-motion-record-tests
ctest --test-dir .build/ace-combat-6 --output-on-failure \
  -R '^ac6-motion-record-tests$'
```

Résultat : test ciblé **1/1 PASS**. La régression couvre un type `1`, les deux
relogements de pointeurs, les dispatchs bruts, le bit `+0x08`, le pointeur
optionnel, un enfant non-type-`1` sans mutation et le rejet atomique d'une
table tronquée.

## Frontière

Les dispatchs pointés par `0x82119dd8` et `0x82119f68` sont seulement
sélectionnés. Leur exécution, leurs types concrets et leur consommation par la
mission ou le moteur de vol restent `needs-types` / `needs-dynamic-evidence`.
