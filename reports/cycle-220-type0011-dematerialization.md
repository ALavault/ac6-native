# AC6 cycle 220 — dématérialisation type `0x0011`

## Identité et preuves

- Cible : `ac6-xbox360-pal`
- Module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Projet Ghidra canonique : `ace-combat-6`
- Exports headless : `exports/82339798.json`, `exports/82345728.json`,
  `exports/8211bcd8.json`
- Fonctions retail : `0x82339798`, `0x82345728`, dispatcher `0x8211bcd8`

`0x8211bcd8` route un record taggé `0x0011` vers `0x82339798`. Ce wrapper
agit seulement lorsqu'un record non nul porte le bit `0x80000000` à `+0x18`.
Pour le tag `0x0011`, il appelle `0x82345728`, puis efface ce bit.

`0x82345728` lit le compteur à `record+0x14`, parcourt le tableau de mots 32
bits à `record+0x20`, efface `child+0x0c`, puis remplace chaque adresse enfant
absolue par son offset relatif au record. C'est l'inverse observable du
normaliseur `0x823456d0` déjà représenté dans le cycle 218.

## Implémentation native bornée

`denormalize_function_82345728_type_0011` effectue la mutation big-endian
bornée. Il prévalide chaque entrée de table et chaque champ enfant avant la
première écriture. `dematerialize_function_82339798` encode le gate de flag,
l'appel conditionnel du dénormaliseur et l'effacement du flag.

Les protections hôte rejettent les adresses enfant sous le record, les plages
invitées invalides et les dépassements d'adresse; elles ne prétendent pas
décrire une faute retail sur mémoire XEX mal formée. Le tag hors `0x0011`
efface seulement le flag, exactement comme le wrapper observé, sans modifier
le tableau ou les enfants.

## Validation

La régression `ac6-motion-record-tests` couvre :

- deux adresses enfants absolues rétablies en offsets relatifs ;
- l'effacement de leurs deux champs `+0x0c` ;
- l'effacement du flag materialized après dénormalisation ;
- le second appel sans effet une fois le flag effacé ;
- le tag non-`0x0011`, qui conserve tableau et enfants ;
- le record nul ;
- un second enfant tronqué : rejet complet, sans mutation partielle ni
  effacement du flag.

Exécuté :

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
GPU, GUI, VNC, asset retail ou geste humain n'a été utilisé.

## Frontière

La paire matérialisation/dématérialisation couvre la représentation du payload
`0x0011`, non son type métier. L'identité des enfants, la valeur de leur
champ `+0x0c` et la consommation mission/vol restent `needs-types` ou
`needs-dynamic-evidence`.
