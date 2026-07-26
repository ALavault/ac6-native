# AC6 cycle 227 — copie vectorielle bornée du premier dispatch `0x8211A148`

## Identité et preuve

- Cible : `ac6-xbox360-pal`, module `default.xex`
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Projet Ghidra canonique : `ace-combat-6`
- Fonction : `0x8211A148`, premier mot de dispatch produit par `0x82119740`
  pour les sous-entrées de type `1`.

Le décompilateur est partiel, mais le dump d'assembleur headless de 57
instructions qualifie trois chemins complets :

1. `lhz r10,0x8(r3)` lit les flags, et `lwz r11,0x14(r3)` désigne quatre mots
   source ;
2. flags bit 0 et bit 1 posés : quatre `lfs/stfs` sont copiés et le curseur
   pointé par `r4` avance de 16 octets ;
3. bit 0 absent, ou bit 0 seul avec l'octet bas de `r6` nul : trois
   `lfs/stfs` sont copiés et ce curseur avance de 12 octets.

La branche bit 0 seul / `r6 != 0` entre dans une région que le dump ne décode
pas entre `0x8211A1E4` et `0x8211A27C`. Elle ne reçoit donc aucune sémantique
inventée.

## Implémentation native

`copy_function_8211a148_vector` représente les chemins visibles sur un
`MotionRecordGuestView` :

- copie les mots big-endian bruts, préservant bits flottants, NaN et signed
  zero sans passer par la `float` hôte ;
- prévalide source, cellule de curseur et destination avant la première
  écriture ;
- avance le curseur de 12 ou 16 octets seulement après la copie complète ;
- retourne `unresolved_control_path` sans mutation pour la branche opaque.

Le paramètre `cursor_word_guest_address` est un adaptateur borné de la cellule
de curseur observée via `r4`; il ne prétend pas qualifier le type C++ retail de
`r4`. La fonction ne suit pas les dispatchs, ne sérialise pas une primitive, et
ne lui attribue aucun rôle de scène, d'avion ou de vol.

## Validation

La régression `ac6-motion-record-tests` couvre :

- copie de trois mots et avance de 12 octets ;
- copie de quatre mots et avance de 16 octets ;
- sortie sans mutation de la branche `r6 != 0` non décodée ;
- destination tronquée, rejetée sans mutation.

```bash
cmake --build .build/ace-combat-6 -j16 --target ac6-motion-record-tests
ctest --test-dir .build/ace-combat-6 --output-on-failure \
  -R '^ac6-motion-record-tests$'
cmake --build .build/ace-combat-6 -j16
ctest --test-dir .build/ace-combat-6 --output-on-failure -j16
cmake --install .build/ace-combat-6 --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

Résultat ciblé : **1/1 PASS**. Corpus AC6 complet : **42/42 PASS**.
L'installation reste directement sous `bin/`, sans `bin/bin`. Aucun Xenia,
GUI, VNC, asset retail ou intervention humaine n'est utilisé.

## Frontière

Les dispatchs suivants, le contenu de la région non décodée et la consommation
par la mission restent `needs-types` / `needs-dynamic-evidence`. Cette tranche
ne suffit pas à déclarer le chainage de vol ou de rendu vérifié.
